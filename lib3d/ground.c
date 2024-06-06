
#include <stdlib.h>
#include <pd_api.h>
#include <limits.h>
#include <float.h>
#include "ground.h"
#include "perlin.h"
#include "gfx.h"
#include "3dmath.h"
#include "realloc.h"
#include "scales.h"
#include "tracks.h"
#include "models.h"
#include "spall.h"

#define Z_NEAR 0.5f
#define Z_FAR 64.f

static PlaydateAPI* pd;

#define GROUNDFACE_FLAG_QUAD 1
#define GROUNDFACE_FLAG_MATERIAL_MASK 6
#define GROUNDFACE_FLAG_SNOW 2
#define GROUNDFACE_FLAG_ROCK 4

// ground face
// normal + material only
typedef struct {
    Point3d n;
    int flags;
} GroundFace;

// ground tile
typedef struct {
    int prop_id;
    float prop_t;
    float h;
    GroundFace f0;
    GroundFace f1;
} GroundTile;

typedef struct {
    // main track position
    int extents[2];
    int is_checkpoint;
    // tracks positions as bitmask
    uint32_t tracks_mask;
    float y;
    // main track extents
    float center;

    // tile
    GroundTile tiles[GROUND_SIZE];
} GroundSlice;

// active slices + misc "globals"
typedef struct {
    // 
    int slice_id;
    // start slice index
    int plyr_z_index;
    int max_pz;
    float slice_y;
    float noise_y_offset;
    float y_offset;
    // active tracks
    Tracks* tracks;

    GroundSlice* slices[GROUND_SIZE];
} Ground;

typedef struct {
    int w, h;
    LCDBitmap* image;
} PropImage;

typedef struct {
    // contains all zoom levels
    PropImage scaled[_scaled_image_count];
} ScaledProp;

struct {
    // animation frames
    ScaledProp frames[5];
} _coin_frames;

#define PROP_FLAG_HITABLE   1
#define PROP_FLAG_COLLECT   2
#define PROP_FLAG_KILL      4
#define PROP_FLAG_3D        8
#define PROP_FLAG_JUMP      16

typedef struct {
    int flags;
    float radius;
    ThreeDModel* model;
} PropProperties;

static PropProperties _props_properties[NEXT_PROP_ID + 1];

// max. number of props on a given slice (e.g. number of tracks + 1)
#define MAX_PROPS 4
static struct {
    int n;
    PropInfo props[MAX_PROPS];
} _props_info;

// 3d objects owned by lua but rendered by C
#define MAX_RENDER_PROPS 16
typedef struct {
    // property id
    int id;
    // transformation matrix
    float m[16];
} RenderProp;

static struct {
    int n;
    RenderProp props[MAX_RENDER_PROPS];
} _render_props;


// must be next
#define PROP_COIN               NEXT_PROP_ID

// raycasting angles
#define RAYCAST_PRECISION 196
static float _raycast_angles[RAYCAST_PRECISION];

// global buffer to store slices
static GroundSlice _slices_buffer[GROUND_SIZE];
// active ground
static Ground _ground;
static int _warming_up;

static GroundParams active_params;

// 16 32 * 4 bytes bitmaps
static uint32_t _dithers[32 * 16];

// 16 32 * 8 bytes bitmaps (duplicated on x)
static uint8_t _dither_ramps[8 * 32 * 16];

static uint32_t* _backgrounds[61];
static int _backgrounds_heights[61];

// compute normal and assign material for faces
static int _z_offset = 0;
static void mesh_slice(int j) {
    // BEGIN_FUNC();

    _z_offset++;
    GroundSlice* s0 = _ground.slices[j];
    GroundSlice* s1 = _ground.slices[j + 1];

    // base slope normal
    Point3d sn = { .x = 0, .y = GROUND_CELL_SIZE, .z = s0->y - s1->y };
    v_normz(sn.v);

    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        GroundTile* t0 = &s0->tiles[i];
        const Point3d v0 = { .v = {(float)i * GROUND_CELL_SIZE,t0->h + s0->y,(float)j * GROUND_CELL_SIZE} };
        // v1-v0
        const Point3d u1 = { .v = {(float)GROUND_CELL_SIZE,s0->tiles[i + 1].h + s0->y - v0.y,0.f} };
        // v2-v0
        const Point3d u2 = { .v = {(float)GROUND_CELL_SIZE,s1->tiles[i + 1].h + s1->y - v0.y,(float)GROUND_CELL_SIZE} };
        // v3-v0
        const Point3d u3 = { .v = {0.f,s1->tiles[i].h + s1->y - v0.y,(float)GROUND_CELL_SIZE} };

        Point3d n0, n1;
        v_cross(u3.v, u2.v, n0.v);
        v_cross(u2.v, u1.v, n1.v);
        v_normz(n0.v);
        v_normz(n1.v);
        GroundFace* f0 = &t0->f0;
        f0->n = n0;
        f0->flags = n0.y < 0.75f ? GROUNDFACE_FLAG_ROCK : GROUNDFACE_FLAG_SNOW;
        if (v_dot(n0.v, n1.v) < 0.999f) {
            GroundFace* f1 = &t0->f1;
            f1->n = n1;
            f1->flags = n1.y < 0.75f ? GROUNDFACE_FLAG_ROCK : GROUNDFACE_FLAG_SNOW;;
        }
        else {
            f0->flags |= GROUNDFACE_FLAG_QUAD;
        }
    }

    // END_FUNC();
}

static void make_slice(GroundSlice* slice, float y, int warmup) {
    // BEGIN_FUNC();
    static int trees[] = { PROP_TREE0,PROP_TREE1,PROP_TREE_SNOW };

    // smooth altitude changes
    _ground.slice_y = lerpf(_ground.slice_y, y, 0.2f);
    y = _ground.slice_y;
    // capture height
    slice->y = y;
    slice->tracks_mask = 0;

    update_tracks(warmup);

    // generate height
    uint32_t shadow_mask = 0;
    for (int i = 0; i < GROUND_SIZE; ++i) {
        GroundTile* tile = &slice->tiles[i];
        tile->h = (perlin2d((16.f * i) / GROUND_SIZE, _ground.noise_y_offset, 0.25f, 4)) * 4.f * active_params.slope;
        tile->prop_id = 0;
    }

    _ground.noise_y_offset += (active_params.slope + randf()) / 4.f;

    int imin = 3, imax = GROUND_SIZE - 3;
    float main_track_x = GROUND_CELL_SIZE * (imin + imax) / 2.f;
    int is_checkpoint = 0;
    Tracks* tracks = _ground.tracks;    
    for (int k = 0; k < tracks->n; ++k) {
        Track* t = &tracks->tracks[k];
        // center on track
        const int i0 = t->imin, i1 = t->imax + 1;
        if (t->is_main) {
            main_track_x = t->x;
            imin = i0;
            imax = i1;
            for (int i = i0; i < i1; ++i) {
                // remove props from track
                int prop_id = 0;
                float prop_t = 0.5f;
                switch (_ground.tracks->pattern[i - i0]) {
                case 'W': prop_id = PROP_WARNING; break;
                case 'R': prop_id = PROP_ROCK; break;
                case 'M': prop_id = PROP_COW; prop_t = randf(); break;
                case 'J': prop_id = PROP_JUMPPAD; break;
                case 'S': prop_id = PROP_START; break;
                case 'T': 
                    prop_id = trees[(int)(3.f * randf())]; 
                    prop_t = randf(); 
                    // tree shading
                    shadow_mask |= 0x3 << i;
                    break;
                default:
                    ;
                }

                slice->tiles[i].prop_id = prop_id;
                slice->tiles[i].prop_t = prop_t;
                slice->tracks_mask |= 1 << i;
            }
        }
        else {
            // side tracks are less obvious
            for (int i = i0; i < i1; ++i) {
                slice->tracks_mask |= 1 << i;
            }
        }

        // random filling
        for (int i = 3; i < i0 - 1; i++) {
            GroundTile* tile = &slice->tiles[i];
            if (randf() > active_params.props_rate) {
                tile->prop_id = trees[(int)(3.f * randf())];
                tile->prop_t = randf();
                // slight shading under trees
                shadow_mask |= 0x3 << i;
            }
        }
        for (int i = i1 + 1; i < GROUND_SIZE - 2; i++) {
            GroundTile* tile = &slice->tiles[i];
            if (randf() > active_params.props_rate) {
                tile->prop_id = trees[(int)(3.f * randf())];
                tile->prop_t = randf();
                // slight shading under trees
                shadow_mask |= 0x3 << i;
            }
        }
    }

    slice->center = main_track_x;
    slice->extents[0] = imin;
    slice->extents[1] = imax;

    slice->is_checkpoint = is_checkpoint;

    // side walls
    slice->tiles[0].h = 15.f + 5.f * randf();
    slice->tiles[GROUND_SIZE - 1].h = 15.f + 5.f * randf();
    if (active_params.tight_mode) {
        for (int i = 1; i < imin - 1; i++) {
            slice->tiles[i].h = slice->tiles[0].h;
            slice->tiles[i].prop_id = 0;
        }
        for (int i = GROUND_SIZE - 2; i > imax + 1; i--) {
            slice->tiles[i].h = slice->tiles[GROUND_SIZE - 1].h;
            slice->tiles[i].prop_id = 0;
        }
        imin--;
        imax++;
        // kill track shading
        slice->tracks_mask = 0;
    }
    else
    {
        imin = 2;
        imax = GROUND_SIZE - 2;
    }

    // apply props shadows
    slice->tracks_mask |= shadow_mask;
    //  + nice transition
    slice->tiles[imin].h = lerpf(slice->tiles[imin - 1].h, slice->tiles[imin].h, lerpf(0.666f,0.8f,randf()));
    slice->tiles[imax].h = lerpf(slice->tiles[imax + 1].h, slice->tiles[imax].h, lerpf(0.666f,0.8f,randf()));

    _ground.slice_id++;

    // END_FUNC();
}

void make_ground(GroundParams params) {
    active_params = params;
    // reset global params
    _ground.slice_id = 0;
    _ground.slice_y = 0;
    _ground.y_offset = 0;
    _ground.noise_y_offset = 16.f * randf();
    _ground.plyr_z_index = GROUND_SIZE / 2 - 1;
    _ground.max_pz = 0;

    // init track generator
    make_tracks(4 * GROUND_CELL_SIZE, (GROUND_SIZE - 5)*GROUND_CELL_SIZE, params, &_ground.tracks);

    for (int i = 0; i < GROUND_SIZE; ++i) {
        // reset slices
        _ground.slices[i] = &_slices_buffer[i];
        make_slice(_ground.slices[i], -i * params.slope, 1);
    }
    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        mesh_slice(i);
    }
}

void update_ground(Point3d* p, int* slice_id, char** pattern, Point3d* offset) {
    BEGIN_FUNC();

    // prevent going up slope!
    if (p->z < 8 * GROUND_CELL_SIZE) p->z = 8 * GROUND_CELL_SIZE;
    offset->v[0] = 0.f;
    offset->v[1] = 0.f;
    offset->v[2] = 0.f;
    float pz;
    // todo: make sure to handle faster than 1 tile moves
    while ((pz = p->z / GROUND_CELL_SIZE) > _ground.plyr_z_index) {
        // shift back
        p->z -= GROUND_CELL_SIZE;
        offset->z -= GROUND_CELL_SIZE;
        _ground.max_pz -= GROUND_CELL_SIZE;
        GroundSlice* old_slice = _ground.slices[0];
        float old_y = old_slice->y;
        // drop slice 0
        for (int i = 1; i < GROUND_SIZE; ++i) {
            _ground.slices[i - 1] = _ground.slices[i];
            _ground.slices[i - 1]->y -= old_y;
        }
        // move shifted slices back to top
        _ground.slices[GROUND_SIZE - 1] = old_slice;

        // use previous baseline
        make_slice(old_slice, _ground.slices[GROUND_SIZE - 2]->y - active_params.slope * (randf() + 0.5f), 0);
        mesh_slice(GROUND_SIZE - 2);
    }
    // update y offset
    if (p->z > _ground.max_pz) {
        _ground.y_offset = lerpf(_ground.slices[0]->y, _ground.slices[1]->y, pz - (int)pz);
        offset->y = _ground.y_offset;
        _ground.max_pz = (int)p->z;
    }
    *slice_id = _ground.slice_id;
    *pattern = _ground.tracks->pattern;

    END_FUNC();
}

void get_start_pos(Point3d* out) {
    out->x = _ground.slices[_ground.plyr_z_index]->center;
    out->y = 0;
    out->z = (float)_ground.plyr_z_index * GROUND_CELL_SIZE;
}

int get_face(Point3d pos, Point3d* nout, float* yout) {
    // BEGIN_FUNC();

    // z slice
    int i = (int)(pos.x / GROUND_CELL_SIZE), j = (int)(pos.z / GROUND_CELL_SIZE);
    // outside ground?
    if (i < 0 || i >= GROUND_SIZE || j < 0 || j >= GROUND_SIZE) {
        // END_FUNC();
        return 0;
    }

    GroundSlice* s0 = _ground.slices[j];
    GroundTile* t0 = &s0->tiles[i];
    GroundFace* f0 = &t0->f0;
    GroundFace* f1 = &t0->f1;
    GroundFace* f = f0;
    // select face
    if (!(f0->flags & GROUNDFACE_FLAG_QUAD) && (pos.z - GROUND_CELL_SIZE * j < pos.x - GROUND_CELL_SIZE * i)) f = f1;

    // intersection point
    Point3d ptOnFace;
    make_v((Point3d) { .v = {(float)i * GROUND_CELL_SIZE, t0->h + s0->y - _ground.y_offset, (float)j * GROUND_CELL_SIZE} }, pos, &ptOnFace);
         
    // height
    *yout = pos.y - v_dot(ptOnFace.v, f->n.v) / f->n.y;
    // face normal
    *nout = f->n;

    // END_FUNC();

    return 1;
}

// get slice extents
void get_track_info(Point3d pos, float* xmin, float* xmax, float* z, int* checkpoint, float* angleout) {
    BEGIN_FUNC();

    int j = (int)(pos.z / GROUND_CELL_SIZE);
    const GroundSlice* s0 = _ground.slices[j];
    *xmin = (float)(s0->extents[0] * GROUND_CELL_SIZE);
    *xmax = (float)(s0->extents[1] * GROUND_CELL_SIZE);
    // activate checkpoint at middle of cell
    *z = (j + 0.5f) * GROUND_CELL_SIZE;
    *checkpoint = s0->is_checkpoint;


    // find nearest checkpoint
    float x = _ground.slices[j + 2]->center, y = 2;
    for (int k = j + 2; k < j + 10 && k < GROUND_SIZE; ++k) {
        GroundSlice* s = _ground.slices[k];
        if (s->is_checkpoint) {
            x = s->center;
            y = (float)(k - j);
            break;
        }
    }
    // direction to track ahead (rebase to half circle)
    *angleout = 0.5f * atan2f(y * GROUND_CELL_SIZE, x - pos.x) / PI - 0.25f;

    END_FUNC();
}

void get_props(Point3d pos, PropInfo** info, int* nout) {
    const int ii = (int)(pos.x / GROUND_CELL_SIZE);
    int i0 = ii - 4, i1 = ii + 4;
    if (i0 < 0) i0 = 0;
    if (i1 > GROUND_SIZE) i1 = GROUND_SIZE;

    const int j0 = (int)(pos.z / GROUND_CELL_SIZE) + 1;
    const float y_offset = _ground.y_offset;
    _props_info.n = 0;
    for (int j = j0; j < j0 + 3 && _props_info.n < MAX_PROPS; ++j) {
        const GroundSlice* s0 = _ground.slices[j];
        const GroundSlice* s1 = _ground.slices[j + 1];
        for (int i = i0; i < i1 && _props_info.n < MAX_PROPS; ++i) {
            const GroundTile* t0 = &s0->tiles[i];
            int prop_id = t0->prop_id;
            if (prop_id == PROP_COIN) {
                PropInfo* info = &_props_info.props[_props_info.n++];
                info->type = prop_id;                
                v_lerp(
                    &(Point3d) { {.v = { (float)i * GROUND_CELL_SIZE,         t0->h + s0->y - y_offset,       (float)j * GROUND_CELL_SIZE } } },
                    &(Point3d) { {.v = { (float)(i + 1) * GROUND_CELL_SIZE,   s1->tiles[i + 1].h + s1->y - y_offset,   (float)(j + 1) * GROUND_CELL_SIZE } } },
                    t0->prop_t,
                    &info->pos);
                info->pos.z += 2.f;
            }
        }
    }    
    *nout = _props_info.n;
    *info = _props_info.props;
}

// clear checkpoint
void clear_checkpoint(Point3d pos) {
    int j = (int)(pos.z / GROUND_CELL_SIZE);
    GroundSlice* s0 = _ground.slices[j];
    s0->is_checkpoint = 0;
}

void collide(Point3d pos, float radius, int* hit_type)
{
    BEGIN_FUNC();
    // z slice
    int i0 = (int)(pos.x / GROUND_CELL_SIZE), j0 = (int)(pos.z / GROUND_CELL_SIZE);

    // default
    *hit_type = 0;

    // out of track
    if (i0 <= 0 || i0 >= GROUND_SIZE - 2) {
        *hit_type = 2;
        END_FUNC();
        return;
    }

    // square radius
    radius *= radius;

    // check all 9 cells(overkill but faster)
    for (int j = j0 - 1; j < j0 + 2; j++) {
        if (j >= 0 && j < GROUND_SIZE) {
            GroundSlice* s0 = _ground.slices[j];
            for (int i = i0 - 1; i < i0 + 2; i++) {
                if (i >= 0 && i < GROUND_SIZE) {
                    GroundTile* t0 = &s0->tiles[i];
                    int id = t0->prop_id;
                    // collidable actor ?
                    if (id) {
                        PropProperties* props = &_props_properties[id - 1];
                        if (props->flags & PROP_FLAG_HITABLE) {

                            // generate vertex
                            Point3d v0 = (Point3d){ .v = {(float)(i * GROUND_CELL_SIZE),t0->h + s0->y - _ground.y_offset,(float)(j * GROUND_CELL_SIZE)} };
                            Point3d v2 = (Point3d){ .v = {(float)((i + 1) * GROUND_CELL_SIZE),s0->tiles[i + 1].h + s0->y - _ground.y_offset,(float)((j + 1) * GROUND_CELL_SIZE)} };
                            Point3d res;
                            v_lerp(&v0, &v2, t0->prop_t, &res);
                            make_v(pos, res, &res);
                            if (res.x * res.x + res.z * res.z < radius + props->radius * props->radius) {
                                if (props->flags & PROP_FLAG_COLLECT) {
                                    // "collect" prop
                                    t0->prop_id = 0;
                                    *hit_type = 3;
                                    END_FUNC();
                                    return;
                                }
                                // insta-kill?
                                if (props->flags & PROP_FLAG_KILL) {
                                    *hit_type = 2;
                                }
                                else if (props->flags & PROP_FLAG_JUMP) {
                                    *hit_type = 4;
                                }
                                else {
                                    *hit_type = 1;
                                }
                                END_FUNC();
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
    END_FUNC();
}

/*
* loading assets helpers
*/
static void free_bitmap_table(void* data, const int _, const int __) {
    pd->graphics->freeBitmapTable((LCDBitmapTable*)data);
}

static void load_noise(void* ptr,const int i, const int _) {
    LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, i);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (w != 32 || h != 32)
        pd->system->logToConsole("Invalid noise image format: %dx%d", w, h);

    // copy "regular" dither
    memcpy((uint8_t*)(_dithers + i * 32), data, 32 * sizeof(uint32_t));

    // organized by ramps
    for (int j = 0; j < 32; ++j) {
        for (int k = 0; k < 4; k++, data++) {
            // interleaved values (4 bytes)
            _dither_ramps[i * 8 + j * 16 * 8 + k] = *data;
            _dither_ramps[i * 8 + j * 16 * 8 + k + 4] = *data;
        }
    }
}

static void load_background(void* ptr, const int angle, const int _) {
    const int image_index = angle - _scaled_image_min_angle;
    LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, image_index);
    // convert to "angle"
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (!bitmap) {
        pd->system->error("Missing background at angle: %i", angle);
    }

    if (w != 400)
        pd->system->logToConsole("Invalid background format: %ix%i, expected 400x*", w, h);
    uint32_t* image = lib3d_malloc(h * LCD_ROWSIZE);
    uint32_t* dst = image;
    for (int j = 0; j < h; ++j) {
        // create 32bits blocks
        for (int i = 0; i < 12; ++i) {            
            *(dst++) = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
            data += 4;
        }
        // last 16 bits
        *(dst++) = (data[1] << 8) | data[0];
        data += 2;
    }    
    _backgrounds[image_index] = image;
    _backgrounds_heights[image_index] = h;
}

static void load_coins(void* ptr, const int _1, const int _2) {    
    for (int frame = 0; frame < 5; frame++) {
        for (int i = 0; i < _scaled_image_count; i++) {
            int w, h, stride;
            uint8_t* data, * alpha;
            LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, frame * _scaled_image_count + i);
            if (!bitmap) {
                pd->system->error("Missing coin frame: %i scale: %i", frame, i);
            }
            pd->graphics->getBitmapData(bitmap, &w, &h, &stride, &alpha, &data);

            _coin_frames.frames[frame].scaled[i] = (PropImage){
                .w = w,
                .h = h,
                .image = bitmap
            };
        }
    }
}

typedef void(* unit_of_work_callback)(void*, const int, const int);
typedef struct {
    unit_of_work_callback callback;
    void* param0;
    int param1;
    int param2;
} UnitOfWork;

static struct {
    UnitOfWork todo[256];
    int cursor;
    int n;
} _work;


void ground_init(PlaydateAPI* playdate) {

    // keep SDK handle   
    pd = playdate;

    // local strings
    const char* err;
    char* path = NULL;
    LCDBitmapTable* bitmaps = NULL;

    _work.cursor = 0;
    _work.n = 0;

    // read animated & scaled coins
    {
        pd->system->logToConsole("Loading coins");
        bitmaps = pd->graphics->loadBitmapTable("images/generated/coin", &err);
        if (!bitmaps)
            pd->system->logToConsole("Failed to load: %s, %s", path, err);

        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_coins,
            .param0 = bitmaps,
            .param1 = -1,
            .param2 = -1
        };
    }

    // read dither table
    pd->system->formatString(&path, "images/generated/noise32x32");
    pd->system->logToConsole("Loading noise: %s", path);
    bitmaps = pd->graphics->loadBitmapTable(path, &err);
    
    if (!bitmaps)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);

    for (int i = 0; i < 16; ++i) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_noise,
            .param0 = bitmaps,
            .param1 = i,
            .param2 = -1
        };
    }
    _work.todo[_work.n++] = (UnitOfWork){
        .callback = free_bitmap_table,
        .param0 = bitmaps,
        .param1 = -1,
        .param2 = -1
    };

    // read rotated backgrounds
    pd->system->formatString(&path, "images/generated/sky_background");
    pd->system->logToConsole("Loading background: %s", path);
    bitmaps = pd->graphics->loadBitmapTable(path, &err);
    if (!bitmaps)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);

    for (int i = _scaled_image_min_angle; i <= _scaled_image_max_angle; i++) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_background,
            .param0 = bitmaps,
            .param1 = i,
            .param2 = -1
        };
    }
    _work.todo[_work.n++] = (UnitOfWork){
        .callback = free_bitmap_table,
        .param0 = bitmaps,
        .param1 = -1,
        .param2 = -1
    };

    pd->system->logToConsole("Load async tasks #: %i", _work.n);

    // raycasting angles
    for(int i=0;i<RAYCAST_PRECISION;++i) {
        float t = 2.f * (((float)i) / RAYCAST_PRECISION - 0.5f);
        _raycast_angles[i] = atan2f(Z_NEAR, Z_NEAR * t * 2.f) - PI / 2.f;
    }

    // props config (todo: get from lua?)

    // pine tree
    _props_properties[PROP_TREE0 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f};
    _props_properties[PROP_TREE0 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    _props_properties[PROP_TREE_SNOW - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    // checkpoint flags
    _props_properties[PROP_CHECKPOINT_LEFT - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    _props_properties[PROP_CHECKPOINT_RIGHT - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    // obstacles
    _props_properties[PROP_ROCK - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_KILL, .radius = 3.f };
    _props_properties[PROP_COW - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_KILL, .radius = 2.5f };
    // snowball
    _props_properties[PROP_SNOWBALL - 1] = (PropProperties){ .flags = PROP_FLAG_KILL, .radius = 2.f };
    _props_properties[PROP_SPLASH - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    // 
    _props_properties[PROP_JUMPPAD - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_JUMP, .radius = 2.0f };
    // coin
    _props_properties[PROP_COIN - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_COLLECT, .radius = 2.0f };

    // bind all props to the corresponding 3d model
    for (int i = 0; i < NEXT_PROP_ID; ++i) {
        _props_properties[i].flags |= PROP_FLAG_3D;
        _props_properties[i].model = &three_d_models[i];
    }
}

int ground_load_assets_async() {

    // done
    if (_work.cursor >= _work.n) {
        return 0;        
    }

    UnitOfWork* activeUnit = &_work.todo[_work.cursor++];
    (*activeUnit->callback)(activeUnit->param0, activeUnit->param1, activeUnit->param2);
    return 1;
}

// --------------- 3d rendering -----------------------------
#define OUTCODE_IN 0
#define OUTCODE_FAR 1
#define OUTCODE_NEAR 2
#define OUTCODE_RIGHT 4
#define OUTCODE_LEFT 8

// todo: move to settings?
#define SHADING_CONTRAST 1.5f

#define MAX_TILE_DIST 18

typedef struct {
    float x;
    float y;
    float z;
    int outcode;
} CameraPoint3d;

// visible tiles encoded as 1 bit per cell
static uint32_t _visible_tiles[GROUND_SIZE];

typedef struct {
    // texture type
    int material;
    // original flags
    int flags;
    // number of points
    int n;
    // clipped points in camera space
    Point3du pts[5];
} DrawableFace;

typedef struct {
    int material;
    int angle;
    Point3d pos;
} DrawableProp;

typedef struct {
    int frame;
    Point3d pos;
} DrawableCoin;

struct Drawable_s;
typedef void(*draw_drawable)(struct Drawable_s* drawable, uint8_t* bitmap);

// generic drawable thingy
// cache-friendlyness???
typedef struct Drawable_s {
    float key;
    draw_drawable draw;
    union {
        DrawableFace face;
        DrawableProp prop;
        DrawableCoin coin;
    };
} Drawable;

static struct {
    int n;
    // arbitrary limit
    Drawable all[GROUND_SIZE * GROUND_SIZE * 4];
} _drawables;

static Drawable* _sortables[GROUND_SIZE * GROUND_SIZE * 4];

// visible tiles encoded as 1 bit per cell
static uint32_t _visible_tiles[GROUND_SIZE];

// cache entry (transformed point in camera space)
typedef struct {
    Point3du p;
    int outcode;
} CameraPoint;

typedef struct {
    int i, j;
    uint32_t mask;
    float h;
    float y;
    CameraPoint* cache;
} GroundSliceCoord;

// compare 2 drawables
static int cmp_drawable(const void* a, const void* b) {
    const float x = (*(Drawable**)a)->key;
    const float y = (*(Drawable**)b)->key;
    return x < y ? -1 : x == y ? 0 : 1;
}

// clip polygon against near-z
static int z_poly_clip(const float znear, Point3du* in, int n, Point3du* out) {
    BEGIN_FUNC();
    Point3du v0 = in[n - 1];
    float d0 = v0.z - znear;
    int nout = 0;
    for (int i = 0; i < n; i++) {
        Point3du v1 = in[i];
        int side = d0 > 0;
        if (side) out[nout++] = (Point3du){ .v = { v0.x, v0.y, v0.z }, .u = v0.u, .light = v0.light };
        const float d1 = v1.z - znear;
        if ((d1 > 0) != side) {
            // clip!
            const float t = d0 / (d0 - d1);
            out[nout++] = (Point3du){ .v = {
                lerpf(v0.x,v1.x,t),
                lerpf(v0.y,v1.y,t),
                znear},
                .u = lerpf(v0.u,v1.u,t),
                .light = lerpf(v0.light,v1.light,t)
            };
        }
        v0 = v1;
        d0 = d1;
    }
    END_FUNC();
    return nout;
}

static void draw_tile(struct Drawable_s* drawable, uint8_t* bitmap) {
    // BEGIN_FUNC();

    DrawableFace* face = &drawable->face;

    const int n = face->n;
    Point3du* pts = face->pts;
    for (int i = 0; i < n; ++i) {
        // project 
        float w = 1.f / pts[i].z;
        pts[i].x = 199.5f + 199.5f * w * pts[i].x;
        pts[i].y = 119.5f - 199.5f * w * pts[i].y;
        // works ok
        float shading = face->material == GROUNDFACE_FLAG_SNOW ? 4.0f * pts[i].u + 8.f * w : 4.0f + 4.0f * pts[i].u + 4.f * w;
        // attenuation
        shading *= pts[i].light;
        if (shading > 15.f) shading = 15.f;
        if (shading < 0.f) shading = 0.f;
        pts[i].u = shading;
    }

    // 
    texfill(pts, n, _dither_ramps, bitmap);

    /*
    float x0 = pts[n - 1].x, y0 = pts[n - 1].y;
    for (int i = 0; i < n; ++i) {
        float x1 = pts[i].x, y1 = pts[i].y;
        pd->graphics->drawLine(x0,y0,x1,y1, 1, kColorBlack);
        x0 = x1, y0 = y1;
    }
    */
    // END_FUNC();
}

static void draw_face(Drawable* drawable, uint8_t* bitmap) {
    BEGIN_FUNC();
    DrawableFace* face = &drawable->face;

    const int n = face->n;
    Point3du* pts = face->pts;
    for (int i = 0; i < n; ++i) {
        // project 
        float w = 1.f / pts[i].z;
        pts[i].x = 199.5f + 199.5f * w * pts[i].x;
        pts[i].y = 119.5f - 199.5f * w * pts[i].y;
    }

    // 
    float dist = drawable->key - 12.f * GROUND_CELL_SIZE;
    float shading = dist / (2.f * GROUND_CELL_SIZE);
    if (shading > 1.f) shading = 1.f;
    if (shading < 0.f) shading = 0.f;    
    if (!(face->flags & FACE_FLAG_TRANSPARENT)) {
        polyfill(pts, n, _dithers + (int)(face->material * (1.f - shading)) * 32, (uint32_t*)bitmap);
    }

    // don't "pop" edges if too far away
    if ((face->flags & FACE_FLAG_EDGES) && dist < 0.f) {
        Point3du* p0 = &pts[n - 1];
        for (int i = 0; i < n; ++i) {
            Point3du* p1 = &pts[i];
            if (p0->u) {
                pd->graphics->drawLine(p0->x, p0->y, p1->x, p1->y, 1, kColorBlack);
            }
            p0 = p1;
        }
    }
    END_FUNC();
}

static void draw_coin(Drawable* drawable, uint8_t* bitmap) {
    DrawableCoin* prop = &drawable->coin;
    
    const float w = 199.5f / prop->pos.z;
    const float x = 199.5f + w * prop->pos.x;
    const float y = 119.5f - w * prop->pos.y;
    int i = (int)(_scaled_image_count * (prop->pos.z - Z_NEAR) / GROUND_CELL_SIZE);
    if (i > 255) i = 255;
    PropImage* image = &_coin_frames.frames[prop->frame].scaled[_scaled_by_z[i]];
    pd->graphics->drawBitmap(image->image, (int)(x - image->w / 2), (int)(y - image->h / 2), 0);
}

// push a face to the drawing list
static void push_tile(GroundFace* f, const float* m, GroundSliceCoord* coords, int n, const float light) {
    BEGIN_FUNC();

    Point3du tmp[4];

    // transform
    int outcode = 0xfffffff, is_clipped_near = 0;
    float min_key = FLT_MIN;
    for (int i = 0; i < n; ++i) {
        // check cache entry
        GroundSliceCoord c = coords[i];
        // grab correct point cache line
        CameraPoint* cp = &c.cache[c.i];
        // not fresh?
        if (cp->outcode == -1) {
            Point3du* res = &cp->p;
            // compute point  
            Point3d p = { .v = {(float)c.i * GROUND_CELL_SIZE, c.h + c.y, (float)c.j * GROUND_CELL_SIZE} };
            // project using active matrix
            m_x_v(m, p.v, res->v);
            int code = res->z > Z_NEAR ? OUTCODE_IN : OUTCODE_NEAR;
            if (res->x > res->z) code |= OUTCODE_RIGHT;
            if (-res->x > res->z) code |= OUTCODE_LEFT;
            cp->outcode = code;

            // light
            res->u = (4.0f + c.h) / 8.f;
            // constant: snow contrast
            // shading: track contrast
            res->light = 0.5f + (!!(c.mask & (1 << c.i))) * 0.75f;
        }
        
        outcode &= cp->outcode;
        is_clipped_near |= cp->outcode;
        if (cp->p.z > min_key) min_key = cp->p.z;
        // copy point to tmp array
        tmp[i] = cp->p;
        tmp[i].light *= light;
    }

    // visible?
    if (outcode == 0) {
        Drawable* drawable = &_drawables.all[_drawables.n++];
        drawable->draw = draw_tile;
        drawable->key = min_key;
        DrawableFace* face = &drawable->face;
        face->material = f->flags & GROUNDFACE_FLAG_MATERIAL_MASK;
        if (is_clipped_near & OUTCODE_NEAR) {
            face->n = z_poly_clip(Z_NEAR, tmp, n, face->pts);
        }
        else {
            face->n = n;
            for (int i = 0; i < n; ++i) {
                face->pts[i] = tmp[i];
            }
        }
    }

    END_FUNC();
}

void add_render_prop(int id, const float* m) {
    RenderProp* p = &_render_props.props[_render_props.n++];
    if (_render_props.n>=MAX_RENDER_PROPS)
        pd->system->error("Too many render props: %i/%i", _render_props.n, MAX_RENDER_PROPS);

    p->id = id;
    // todo: check mis-align
    memcpy(p->m, m, MAT4x4 * sizeof(float));
}

static void push_threeD_model(const int prop_id, const Point3d cv, const float* m) {
    BEGIN_FUNC();

    Point3du tmp[4];
    ThreeDModel* model = _props_properties[prop_id - 1].model;
    for (int j = 0; j < model->face_count; ++j) {
        ThreeDFace* f = &model->faces[j];
        // visible?
        if ( v_dot(f->n.v, cv.v) > f->cp) {
            // vert count
            int n = f->flags & FACE_FLAG_QUAD?4:3;
            // transform
            int outcode = 0xfffffff, is_clipped_near = 0;
            float min_key = FLT_MIN;
            for (int i = 0; i < n; ++i) {
                Point3du* res = &tmp[i];
                // project using active matrix
                m_x_v(m, f->vertices[i].v, res->v);
                int code = res->z > Z_NEAR ? OUTCODE_IN : OUTCODE_NEAR;
                if (res->x > res->z) code |= OUTCODE_RIGHT;
                if (-res->x > res->z) code |= OUTCODE_LEFT;
                outcode &= code;
                is_clipped_near |= code;
                if (res->z > min_key) min_key = res->z;
                // use u to mark sharp edges
                res->u = f->edges & (1 << i);
            }

            // visible?
            if (outcode == 0) {
                Drawable* drawable = &_drawables.all[_drawables.n++];
                drawable->draw = draw_face;
                drawable->key = min_key;
                DrawableFace* face = &drawable->face;
                face->flags = f->flags;
                face->material = f->material;
                if (is_clipped_near & OUTCODE_NEAR) {
                    face->n = z_poly_clip(Z_NEAR, tmp, n, face->pts);
                }
                else {
                    face->n = n;
                    for (int i = 0; i < n; ++i) {
                        face->pts[i] = tmp[i];
                    }
                }
            }       
        }
    }
    END_FUNC();
}

static void push_coin(const Point3d* p, int time) {
    static int animation[9] = { 0,1,2,3,4,3,2,1 };
    Drawable* drawable = &_drawables.all[_drawables.n++];
    drawable->draw = draw_coin;
    drawable->key = p->z;
    drawable->coin.pos = *p;
    drawable->coin.frame = animation[time%8];
}

static void push_and_transform_threeD_model(const int prop_id, const Point3d cam_pos, const float* cam_m, const float* m) {
    BEGIN_FUNC();

    float mvv[MAT4x4];
    m_x_m(cam_m, m, mvv);

    // cam pos in 3d model space
    Point3d inv_cam_pos;
    m_inv_x_v(m, cam_pos.v, inv_cam_pos.v);

    push_threeD_model(prop_id, inv_cam_pos, mvv);

    END_FUNC();
}

int render_sky(float* m, uint8_t* screen) {
    BEGIN_FUNC();

    uint32_t* bitmap = (uint32_t*)screen;

    // cam up in world space
    Point3d n = { .v = {-m[4],-m[5],-m[6]} };

    // intersection between camera eye and up plane(world space)
    Point3d p = { .v = { 0.f, -n.z / n.y, 1.f } };

    float w = 199.5f / p.z;
    float y0 = 169.5f - w * p.y;

    // horizon 'normal'
    n.z = 0;
    v_normz(n.v);
    int angle = (int)(90.0f + 180.0f * atan2f(n.y, n.x) / PI);
    if (angle < _scaled_image_min_angle) angle = _scaled_image_min_angle;
    if (angle > _scaled_image_max_angle) angle = _scaled_image_max_angle;
    uint32_t* src = _backgrounds[angle - _scaled_image_min_angle];

    int h = _backgrounds_heights[angle - _scaled_image_min_angle];

    int h0 = (int)(y0 - h / 2);
    if (h0 < 0) {
        src -= h0 * LCD_ROWSIZE / sizeof(uint32_t);
        h0 = 0;
    }
    if (h0 > LCD_ROWS) h0 = LCD_ROWS;

    int h1 = (int)(y0 + h / 2);
    if (h1 < 0) h1 = 0;
    if (h1 > LCD_ROWS) h1 = LCD_ROWS;
    
    memset(bitmap, 0xff, h0 * LCD_ROWSIZE);
    const uint32_t *dither_base = _dithers + 8 * 32;
    bitmap += h0 * LCD_ROWSIZE / sizeof(uint32_t);
    for (int h = h0; h < h1; ++h) {
        const uint32_t dither = dither_base[h & 31];
        for (int i = 0; i < LCD_ROWSIZE / sizeof(uint32_t); ++i, ++src, ++bitmap) {
            *bitmap = (*src & dither) | (~(*src));
        }
    }
    memset(bitmap, 0xff, (LCD_ROWS - h1) * LCD_ROWSIZE);

    END_FUNC();

    return angle;
}

static void collect_tiles(const Point3d pos, float base_angle) {
    BEGIN_FUNC();

    float x = pos.x / GROUND_CELL_SIZE, y = pos.z / GROUND_CELL_SIZE;
    int32_t x0 = (int)x, y0 = (int)y;

    // reset tiles
    memset((uint8_t*)_visible_tiles, 0, sizeof(uint32_t) * GROUND_SIZE);

    // current tile is always in
    _visible_tiles[y0] |= 1 << x0;

    for (int i = 0; i < RAYCAST_PRECISION; ++i) {
        float angle = base_angle + _raycast_angles[i];
        float v = cosf(angle), u = sinf(angle);

        int32_t mapx = x0, mapy = y0;
        int32_t mapdx = 1, mapdy = 1;
        float ddx = 1.f / u, ddy = 1.f / v;
        float distx = 0, disty = 0;
        if (u < 0.f) {
            mapdx = -1;
            ddx = -ddx;
            distx = (x - mapx) * ddx;
        }
        else {
            distx = (mapx + 1 - x) * ddx;
        }
        if (v < 0) {
            mapdy = -1;
            ddy = -ddy;
            disty = (y - mapy) * ddy;
        }
        else {
            disty = (mapy + 1 - y) * ddy;
        }

        for (int dist = 0; dist < MAX_TILE_DIST; ++dist) {
            if (distx < disty) {
                distx += ddx;
                mapx += mapdx;
            }
            else {
                disty += ddy;
                mapy += mapdy;
            }
            // out of range?
            if ( (mapx | mapy) & 0xffffffe0 ) break;

            _visible_tiles[mapy] |= 1 << mapx;
        }
    }

    END_FUNC();
}

// render ground

void render_ground(Point3d cam_pos, const float cam_tau_angle, float* m, uint8_t* bitmap) {
    BEGIN_FUNC();

    // cache lines
    static CameraPoint c0[GROUND_SIZE];
    static CameraPoint c1[GROUND_SIZE];

    CameraPoint* cache[2] = { c0, c1 };
    // reset cache
    for (int i = 0; i < GROUND_SIZE; ++i) {
        c0[i].outcode = -1;
        c1[i].outcode = -1;
    }

    const float cam_angle = cam_tau_angle * 2.f * PI;
    render_sky(m, bitmap);

    // collect visible tiles
    _drawables.n = 0;
    collect_tiles(cam_pos, cam_angle);

    const float y_offset = _ground.y_offset;
    const int time_offset = (int)(pd->system->getCurrentTimeMilliseconds() / 250.f);
    // transform
    for (int j = 0; j < GROUND_SIZE - 1; ++j) {
        uint32_t visible_tiles = _visible_tiles[j];
        // slightly alter shading of even/odd slices
        const float shading_band = SHADING_CONTRAST * (0.5f + 0.5f * ((j + _z_offset) & 1));
        for (int i = 0; i < GROUND_SIZE; ++i) {
            // is the tile bit enabled?
            if (visible_tiles & (1 << i)) {
                GroundSlice* s0 = _ground.slices[j];
                GroundTile* t0 = &s0->tiles[i];
                GroundSlice* s1 = _ground.slices[j + 1];
                const Point3d v0 = {.v = {(float)i * GROUND_CELL_SIZE,         t0->h + s0->y - y_offset,       (float)j * GROUND_CELL_SIZE}};                

                // camera to face point
                const Point3d cv = { .x = v0.x - cam_pos.x,.y = v0.y - cam_pos.y,.z = v0.z - cam_pos.z };
                const GroundFace* f0 = &t0->f0;

                if (f0->flags & GROUNDFACE_FLAG_QUAD) {
                    if (v_dot(f0->n.v, cv.v) < 0.f)
                    {
                        push_tile(f0, m, (GroundSliceCoord[]) {
                            { .h = s0->tiles[i].h,   .y = s0->y - y_offset, .i = i,     .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .h = s0->tiles[i+1].h, .y = s0->y - y_offset, .i = i + 1, .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .h = s1->tiles[i+1].h, .y = s1->y - y_offset, .i = i + 1, .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask },
                            { .h = s1->tiles[i].h,   .y = s1->y - y_offset, .i = i,     .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 4, shading_band);
                    }
                }
                else {
                    if (v_dot(f0->n.v, cv.v) < 0.f)
                    {
                        push_tile(f0, m, (GroundSliceCoord[]) {
                            { .h = s0->tiles[i].h,     .y = s0->y - y_offset, .i = i,      .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .h = s1->tiles[i + 1].h, .y = s1->y - y_offset, .i = i + 1,  .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask },
                            { .h = s1->tiles[i].h,     .y = s1->y - y_offset, .i = i,      .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 3, shading_band);
                    }
                    const GroundFace* f1 = &t0->f1;
                    if (v_dot(f1->n.v, cv.v) < 0.f)
                    {
                        push_tile(f1, m, (GroundSliceCoord[]) {
                            { .h = s0->tiles[i].h,   .y = s0->y - y_offset, .i = i,     .j = j,     .cache = cache[0], .mask = s1->tracks_mask},
                            { .h = s0->tiles[i+1].h, .y = s0->y - y_offset, .i = i + 1, .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .h = s1->tiles[i+1].h, .y = s1->y - y_offset, .i = i + 1, .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 3, shading_band);
                    }
                }
                // draw prop (if any)
                int prop_id = t0->prop_id;
                if (prop_id) {
                    const float t = t0->prop_t;
                    Point3d pos = {
                        .x = v0.x + GROUND_CELL_SIZE * t,
                        .y = v0.y + (s1->tiles[i + 1].h + s1->y - s0->tiles[i].h - s0->y) * t,
                        .z = v0.z + GROUND_CELL_SIZE * t
                    };
                    if (prop_id == PROP_COIN) {
                        // adjust coin height
                        pos.z += 2.f;
                    }
                    Point3d res;
                    m_x_v(m, pos.v, res.v);
                    if (res.z > Z_NEAR && res.z < (float)(GROUND_CELL_SIZE * MAX_TILE_DIST)) {
                        if (prop_id == PROP_COIN) {
                            push_coin(&res, time_offset + j + _z_offset);
                        }
                        else {    
                            const Point3d cv = { .x = cam_pos.x - pos.x,.y = cam_pos.y - pos.y,.z = cam_pos.z - pos.z};
                            // adjust matrix to project into position
                            float mmvm[MAT4x4];
                            m_x_translate(m, pos.v, mmvm);
                            push_threeD_model(prop_id, cv, mmvm);
                        }
                    }
                }
            }
        }
        // swap cache lines
        CameraPoint* tmp = cache[0];
        cache[0] = cache[1];
        cache[1] = tmp;
        for (int i = 0; i < GROUND_SIZE; ++i) {
            tmp[i].outcode = -1;
        }
    }

    // any "free" props?
    for (int i = 0; i < _render_props.n; ++i) {
        RenderProp* prop = &_render_props.props[i];
        push_and_transform_threeD_model(prop->id, cam_pos, m, prop->m);
    }
    // reset "free" props
    _render_props.n = 0;

    // sort & renders back to front
    if (_drawables.n > 0) {
        for (int i = 0; i < _drawables.n; ++i) {
            _sortables[i] = &_drawables.all[i];
        }
        BEGIN_BLOCK("qsort");
        qsort(_sortables, (size_t)_drawables.n, sizeof(Drawable*), cmp_drawable);
        END_BLOCK();

        // rendering
        for (int k = _drawables.n - 1; k >= 0; --k) {
            _sortables[k]->draw(_sortables[k], bitmap);
        }
    }
    END_FUNC();
}

// render "free" props without ground (for title screen say)
void render_props(Point3d cam_pos, float* m, uint8_t* bitmap) {
    BEGIN_FUNC();

    // "free" props?
    _drawables.n = 0;
    for (int i = 0; i < _render_props.n; ++i) {
        RenderProp* prop = &_render_props.props[i];
        push_and_transform_threeD_model(prop->id, cam_pos, m, prop->m);
    }
    // reset "free" props
    _render_props.n = 0;

    // sort & renders back to front
    if (_drawables.n > 0) {
        for (int i = 0; i < _drawables.n; ++i) {
            _sortables[i] = &_drawables.all[i];
        }
        BEGIN_BLOCK("qsort");
        qsort(_sortables, (size_t)_drawables.n, sizeof(Drawable*), cmp_drawable);
        END_BLOCK();

        // rendering
        for (int k = _drawables.n - 1; k >= 0; --k) {
            _sortables[k]->draw(_sortables[k], bitmap);
        }
    }
    END_FUNC();
}
