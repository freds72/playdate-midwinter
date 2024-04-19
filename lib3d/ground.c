
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

#define GROUND_SIZE 32
#define GROUND_CELL_SIZE 4

static PlaydateAPI* pd;

typedef struct {
    float y;
    // faces (normal + material only)
    GroundFace faces[GROUND_SIZE * 2];
    // height
    float h[GROUND_SIZE];
    // actor type (0 if none)
    int props[GROUND_SIZE];
    // ratio [0;1[ in the face diagonal
    float prop_t[GROUND_SIZE];

    // 
    int is_checkpoint;
    // main track extents
    float center;
    float extents[2];

} GroundSlice;

// active slices + misc "globals"
typedef struct {
    // 
    int slice_id;
    float slice_y;
    float noise_y_offset;
    float y_offset;
    int plyr_z_index;
    int max_pz;
    // active tracks
    Tracks* tracks;

    GroundSlice* slices[GROUND_SIZE];
} Ground;

struct {
    int is_active;
    int angle;
    Point3d pos;
} _snowball;

typedef struct {
    int w, h;
    LCDBitmap* image;
} PropImage;

typedef struct {
    // contains all zoom levels
    PropImage scaled[_scaled_image_count];
} ScaledProp;

typedef struct {
    // -30 - 30 (inc.)
    ScaledProp rotated[61];
} RotatedScaledProp;

struct {
    // animation frames
    ScaledProp frames[5];
} _coin_frames;

static const char* _props_paths[] = {
    // 1: pine tree
    "images/generated/pine_snow_0",
    // 2: checkpoint flag
    "images/generated/checkpoint_left",
};

typedef struct {
    int hitable;
    int single_use;
    float radius;
} PropProperties;

static PropProperties _props_properties[5];

#define MATERIAL_SNOW 0
#define MATERIAL_ROCK 1

#define PROP_TREE 1
#define PROP_CHECKPOINT 2
#define PROP_COIN 3
#define PROP_COW 4
#define PROP_ROCK 5


static RotatedScaledProp _ground_props[8];
static PropImage _snowball_frames[360];

// raycasting angles
#define RAYCAST_PRECISION 128
static float _raycast_angles[RAYCAST_PRECISION];

// global buffer to store slices
static GroundSlice _slices_buffer[GROUND_SIZE];
// active ground
static Ground _ground;

static GroundParams active_params;

// 16 32 * 8 bytes bitmaps (duplicated on x)
static uint8_t _dithers[8 * 32 * 16];

static uint32_t* _backgrounds[61];
static int _backgrounds_heights[61];

// compute normal and assign material for faces
static int _z_offset = 0;
static void mesh_slice(int j) {
    _z_offset++;
    GroundSlice* s0 = _ground.slices[j];
    GroundSlice* s1 = _ground.slices[j + 1];

    // base slope normal
    Point3d sn = { .x = 0, .y = GROUND_CELL_SIZE, .z = s0->y - s1->y };
    v_normz(&sn);

    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        const Point3d v0 = { .v = {(float)i * GROUND_CELL_SIZE,s0->h[i] + s0->y,(float)j * GROUND_CELL_SIZE} };
        // v1-v0
        const Point3d u1 = { .v = {(float)GROUND_CELL_SIZE,s0->h[i + 1] + s0->y - v0.y,0.f} };
        // v2-v0
        const Point3d u2 = { .v = {(float)GROUND_CELL_SIZE,s1->h[i + 1] + s1->y - v0.y,(float)GROUND_CELL_SIZE} };
        // v3-v0
        const Point3d u3 = { .v = {0.f,s1->h[i] + s1->y - v0.y,(float)GROUND_CELL_SIZE} };

        Point3d n0, n1;
        v_cross(&u3, &u2, &n0);
        v_cross(&u2, &u1, &n1);
        v_normz(&n0);
        v_normz(&n1);
        GroundFace* f0 = &s0->faces[2 * i];
        f0->n = n0;
        if (v_dot(&n0, &n1) > 0.999f) {
            f0->quad = 1;
            f0->material = n0.y < 0.75f ? MATERIAL_ROCK : MATERIAL_SNOW;
        }
        else {
            f0->quad = 0;
            f0->material = n0.y < 0.75f ? MATERIAL_ROCK : MATERIAL_SNOW;
            GroundFace* f1 = &s0->faces[2 * i + 1];
            f1->n = n1;
            f1->quad = 0;
            f1->material = n1.y < 0.75f ? MATERIAL_ROCK : MATERIAL_SNOW;
        }
    }
}

static void make_slice(GroundSlice* slice, float y) {
    // smooth altitude changes
    _ground.slice_y = lerpf(_ground.slice_y, y, 0.2f);
    y = _ground.slice_y;
    // capture height
    slice->y = y;

    update_tracks();

    // generate height
    for (int i = 0; i < GROUND_SIZE; ++i) {
        slice->h[i] = (8.f*perlin2d((16.f * i) / GROUND_SIZE, _ground.noise_y_offset, 0.1f, 4) - 4.f) * active_params.slope;
        // avoid props on side walls
        slice->props[i] = 0;
        if (i > 0 && i < GROUND_SIZE - 2) {
            if (randf() > active_params.props_rate)
            {
                // todo: more types
                slice->props[i] = PROP_TREE;
                slice->prop_t[i] = randf();
            }
        }
    }
    // todo: control roughness from slope?
    _ground.noise_y_offset += 0.5f;
    // side walls
    slice->h[0] = slice->h[1]/2 + 16.0f;
    slice->h[GROUND_SIZE - 1] = slice->h[GROUND_SIZE - 2]/2 + 16.0f;

    float xmin = 2 * GROUND_CELL_SIZE, xmax = (GROUND_SIZE - 2) * GROUND_CELL_SIZE;
    float main_track_x = (xmin + xmax) / 2.f;
    int is_checkpoint = 0;
    Tracks* tracks = _ground.tracks;
    for (int k = 0; k < tracks->n; ++k) {
        Track* t = &tracks->tracks[k];
        const int ii = (int)(t->x / GROUND_CELL_SIZE);
        const int i0 = ii - 2, i1 = ii + 2;
        if (t->is_main) {
            main_track_x = t->x;
            xmin = (float)i0 * GROUND_CELL_SIZE;
            xmax = (float)i1 * GROUND_CELL_SIZE;
            for (int i = i0; i < i1; ++i) {
                // smooth track
                slice->h[i] = t->h + slice->h[i] / 4.f;
                // remove props from track
                slice->props[i] = 0;
            }
            // race markers
            if (_ground.slice_id % 8 == 0) {
                is_checkpoint = 1;
                // checkoint pole
                slice->props[i0] = PROP_CHECKPOINT;
                slice->props[i1] = PROP_CHECKPOINT;
            }
        }
        else {
            // side tracks
            for (int i = i0; i < i1; ++i) {
                slice->h[i] = (t->h + slice->h[i]) / 2.f;
            }
            // coins
            if (_ground.slice_id % 2 == 0) {
                slice->props[ii] = PROP_COIN;
            }
        }
    }

    slice->center = main_track_x;
    slice->extents[0] = xmin;
    slice->extents[1] = xmax;

    slice->is_checkpoint = is_checkpoint;

    _ground.slice_id++;
}

void make_ground(GroundParams params) {
    pd->system->logToConsole("Ground params:\nslope:%f", params.slope);

    active_params = params;
    // reset global params
    _snowball.is_active = 0;

    _ground.slice_id = 0;
    _ground.slice_y = 0;
    _ground.y_offset = 0;
    _ground.noise_y_offset = 16.f * randf();
    _ground.plyr_z_index = GROUND_SIZE / 2 - 1;
    _ground.max_pz = INT_MIN;

    // init track generator
    int num_tracks = params.num_tracks > 0 ? params.num_tracks : 3;
    make_tracks(2 * GROUND_CELL_SIZE, (GROUND_SIZE - 2)*GROUND_CELL_SIZE, num_tracks, &_ground.tracks);
    update_tracks();

    for (int i = 0; i < GROUND_SIZE; ++i) {
        // reset slices
        _ground.slices[i] = &_slices_buffer[i];
        make_slice(_ground.slices[i], -i * params.slope);
    }
    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        mesh_slice(i);
    }
}

void update_snowball(Point3d pos, int rotation) {
    _snowball.is_active = 1;
    _snowball.pos = pos;
    _snowball.angle = ((rotation%360)+360)%360;
}

void update_ground(Point3d* p) {
    // prevent going up slope!
    if (p->z < 8 * GROUND_CELL_SIZE) p->z = 8 * GROUND_CELL_SIZE;
    float pz = p->z / GROUND_CELL_SIZE;
    if (pz > _ground.plyr_z_index) {
        // shift back
        p->z -= GROUND_CELL_SIZE;
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
        make_slice(old_slice, _ground.slices[GROUND_SIZE - 2]->y - active_params.slope * (randf() + 0.5f));
        mesh_slice(GROUND_SIZE - 2);
    }
    // update y offset
    if (p->z > _ground.max_pz) {
        _ground.y_offset = lerpf(_ground.slices[0]->y, _ground.slices[1]->y, pz - (int)pz);
        _ground.max_pz = (int)p->z;
    }
}

void get_start_pos(Point3d* out) {
    out->x = _ground.slices[_ground.plyr_z_index]->center;
    out->y = 0;
    out->z = (float)_ground.plyr_z_index * GROUND_CELL_SIZE;
}

void get_face(Point3d pos, Point3d* nout, float* yout,float* angleout) {
    // z slice
    int i = (int)(pos.x / GROUND_CELL_SIZE), j = (int)(pos.z / GROUND_CELL_SIZE);
    
    GroundSlice* s0 = _ground.slices[j];
    GroundFace* f0 = &s0->faces[2 * i];
    GroundFace* f1 = &s0->faces[2 * i + 1];
    GroundFace* f = f0;
    // select face
    if (!f0->quad && (pos.z - GROUND_CELL_SIZE * j < pos.x - GROUND_CELL_SIZE * i)) f = f1;

    // intersection point
    Point3d ptOnFace;
    make_v((Point3d) { .v = {(float)i * GROUND_CELL_SIZE, s0->h[i] + s0->y - _ground.y_offset, (float)j * GROUND_CELL_SIZE} }, pos, &ptOnFace);
         
    // height
    *yout = pos.y - v_dot(&ptOnFace, &f->n) / f->n.y;
    // face normal
    *nout = f->n;
    // direction to track ahead (rebase to half circle)
    *angleout = 0.5f * atan2f(2 * GROUND_CELL_SIZE, _ground.slices[j + 2]->center - pos.x) / PI - 0.25f;
}

// get slice extents
void get_track_info(Point3d pos, float* xmin, float *xmax, float*z, int*checkpoint) {
    int j = (int)(pos.z / GROUND_CELL_SIZE);
    const GroundSlice* s0 = _ground.slices[j];
    *xmin = s0->extents[0];
    *xmax = s0->extents[1];
    // activate checkpoint at middle of cell
    *z = (j + 0.5f) * GROUND_CELL_SIZE;
    *checkpoint = s0->is_checkpoint;
}

// clear checkpoint
void clear_checkpoint(Point3d pos) {
    int j = (int)(pos.z / GROUND_CELL_SIZE);
    GroundSlice* s0 = _ground.slices[j];
    s0->is_checkpoint = 0;
}

void collide(Point3d pos, float radius, int* hit_type)
{
    // z slice
    int i0 = (int)(pos.x / GROUND_CELL_SIZE), j0 = (int)(pos.z / GROUND_CELL_SIZE);

    // default
    *hit_type = 0;

    // out of track
    if (i0 <= 0 || i0 >= GROUND_SIZE - 2) {
        *hit_type = 2;
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
                    int id = s0->props[i];
                    // collidable actor ?
                    if (id) {
                        PropProperties* props = &_props_properties[id - 1];
                        if (props->hitable) {

                            // generate vertex
                            Point3d v0 = (Point3d){ .v = {(float)(i * GROUND_CELL_SIZE),s0->h[i] + s0->y - _ground.y_offset,(float)(j * GROUND_CELL_SIZE)} };
                            Point3d v2 = (Point3d){ .v = {(float)((i + 1) * GROUND_CELL_SIZE),s0->h[i + 1] + s0->y - _ground.y_offset,(float)((j + 1) * GROUND_CELL_SIZE)} };
                            Point3d res;
                            v_lerp(&v0, &v2, s0->prop_t[i], &res);
                            make_v(pos, res, &res);
                            if (res.x * res.x + res.z * res.z < radius + props->radius * props->radius) {
                                if (props->single_use) {
                                    // "collect" prop
                                    s0->props[i] = 0;
                                    *hit_type = 3;
                                    return;
                                }
                                *hit_type = 1;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

/*
* loading assets helpers
*/
static struct {
    int n;
    LCDBitmap* all[128];
} _gc_bitmaps;

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

    for (int j = 0; j < 32; ++j) {
        for (int k = 0; k < 4; k++, data++) {
            // interleaved values (4 bytes)
            _dithers[i * 8 + j * 16 * 8 + k] = *data;
            _dithers[i * 8 + j * 16 * 8 + k + 4] = *data;
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

static void load_prop(void* ptr, const int prop, const int _) {
    for (int angle = _scaled_image_min_angle; angle <= _scaled_image_max_angle; angle++) {
        const int r = angle - _scaled_image_min_angle;
        for (int i = 0; i < _scaled_image_count; i++) {
            int w, h, stride;
            uint8_t* data, * alpha;
            LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, r * _scaled_image_count + i);
            if (!bitmap) {
                pd->system->error("Missing prop %i angle: %i scale: %i", prop, angle, i);
            }
            pd->graphics->getBitmapData(bitmap, &w, &h, &stride, &alpha, &data);

            _ground_props[prop - 1].rotated[r].scaled[i] = (PropImage){
                .w = w,
                .h = h,
                .image = bitmap
            };
        }
    }
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

static void load_snowball(void* ptr, const int prop, const int _) {
    for (int angle = 0; angle < 360; angle++) {
        int w, h, stride;
        uint8_t* data, * alpha;
        LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, angle);
        if (!bitmap) {
            pd->system->error("Missing prop %i angle: %i", prop, angle);
        }
        pd->graphics->getBitmapData(bitmap, &w, &h, &stride, &alpha, &data);

        _snowball_frames[angle] = (PropImage){
            .w = w,
            .h = h,
            .image = bitmap
        };
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
    _gc_bitmaps.n = 0;

    // read rotated & scaled sprites
    for (int prop = 1; prop < 3; prop++) {
        const char* path = _props_paths[prop - 1];
        pd->system->logToConsole("Loading prop: %s", path);
        bitmaps = pd->graphics->loadBitmapTable(path, &err);
        if (!bitmaps)
            pd->system->logToConsole("Failed to load: %s, %s", path, err);

        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_prop,
            .param0 = bitmaps,
            .param1 = prop,
            .param2 = -1
        };
    }

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

    // read snowball
    {
        const char* path = "images/generated/tumbling";
        pd->system->logToConsole("Loading prop: %s", path);
        bitmaps = pd->graphics->loadBitmapTable(path, &err);
        if (!bitmaps)
            pd->system->logToConsole("Failed to load: %s, %s", path, err);

        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_snowball,
            .param0 = bitmaps,
            .param1 = 4,
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
        _raycast_angles[i] = atan2f(31.5f,(float)(i-63.5f)) - PI/2.f;
    }

    // props config (todo: get from lua?)
    // pine tree
    _props_properties[PROP_TREE - 1] = (PropProperties){ .hitable = 1, .single_use = 0, .radius = 1.4f };
    // checkpoint flag
    _props_properties[PROP_CHECKPOINT - 1] = (PropProperties){ .hitable = 0, .single_use = 0, .radius = 0.f };
    // coin
    _props_properties[PROP_COIN - 1] = (PropProperties){ .hitable = 1, .single_use = 1, .radius = 1.f };
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
#define Z_NEAR 0.5f
#define OUTCODE_FAR 0
#define OUTCODE_NEAR 2
#define OUTCODE_RIGHT 4
#define OUTCODE_LEFT 8

// todo: move to settings?
#define SHADING_CONTRAST 1.5f

typedef struct {
    float x;
    float y;
    float z;
    int outcode;
} CameraPoint3d;

// visible tiles encoded as 1 bit per cell
static uint32_t _visible_tiles[GROUND_SIZE];

typedef struct {
    float light;
    // texture type
    int material;
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
typedef void(*draw_drawable)(struct Drawable_s* drawable, uint32_t* bitmap);

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
    Drawable all[GROUND_SIZE * GROUND_SIZE * 3];
    int n;
} _drawables;

static Drawable* _sortables[GROUND_SIZE * GROUND_SIZE * 3];

// visible tiles encoded as 1 bit per cell
static uint32_t _visible_tiles[GROUND_SIZE];

// compare 2 drawables
static int cmp_drawable(const void* a, const void* b) {
    const float x = (*(Drawable**)a)->key;
    const float y = (*(Drawable**)b)->key;
    return x < y ? -1 : x == y ? 0 : 1;
}


// clip polygon against near-z
static int z_poly_clip(const float znear, Point3du* in, int n, Point3du* out) {
    Point3du v0 = in[n - 1];
    float d0 = v0.z - znear;
    int nout = 0;
    for (int i = 0; i < n; i++) {
        Point3du v1 = in[i];
        int side = d0 > 0;
        if (side) out[nout++] = (Point3du){ .v = { v0.x, v0.y, v0.z }, .u = v0.u };
        const float d1 = v1.z - znear;
        if ((d1 > 0) != side) {
            // clip!
            const float t = d0 / (d0 - d1);
            out[nout++] = (Point3du){ .v = {
                lerpf(v0.x,v1.x,t),
                lerpf(v0.y,v1.y,t),
                znear},
                .u = lerpf(v0.u,v1.u,t)
            };
        }
        v0 = v1;
        d0 = d1;
    }
    return nout;
}

static void draw_face(struct Drawable_s* drawable, uint32_t* bitmap) {
    DrawableFace* face = &drawable->face;

    const int n = face->n;
    Point3du* pts = face->pts;
    for (int i = 0; i < n; ++i) {
        // project 
        float w = 1.f / pts[i].z;
        pts[i].x = 199.5f + 199.5f * w * pts[i].x;
        pts[i].y = 119.5f - 199.5f * w * pts[i].y;
        // works ok
        float shading = face->material == MATERIAL_SNOW ? 4.0f * pts[i].u + 8.f * w : 4.0f + 4.0f * pts[i].u + 4.f * w;
        shading *= face->light;
        if (shading > 15.f) shading = 15.0f;
        if (shading < 0.f) shading = 0.f;
        pts[i].u = shading;
    }

    // 
    texfill(pts, n, _dithers, bitmap);

    /*
    float x0 = pts[n - 1].x, y0 = pts[n - 1].y;
    for (int i = 0; i < n; ++i) {
        float x1 = pts[i].x, y1 = pts[i].y;
        pd->graphics->drawLine(x0,y0,x1,y1, 1, kColorBlack);
        x0 = x1, y0 = y1;
    }
    */
}

static void draw_prop(Drawable* drawable, uint32_t* bitmap) {
    DrawableProp* prop = &drawable->prop;

    float w = 199.5f / prop->pos.z;
    float x = 199.5f + w * prop->pos.x;
    float y = 119.5f - w * prop->pos.y;
    PropImage* image = &_ground_props[prop->material - 1].rotated[prop->angle + 30].scaled[_scaled_by_z[(int)(16.0f * (prop->pos.z - Z_NEAR) / GROUND_CELL_SIZE)]];
    pd->graphics->drawBitmap(image->image, (int)(x - image->w / 2), (int)(y - image->h), 0);

}

static void draw_coin(Drawable* drawable, uint32_t* bitmap) {
    DrawableCoin* prop = &drawable->coin;
    
    float w = 199.5f / prop->pos.z;
    float x = 199.5f + w * prop->pos.x;
    float y = 119.5f - w * prop->pos.y;
    PropImage* image = &_coin_frames.frames[prop->frame].scaled[_scaled_by_z[(int)(16.0f * (prop->pos.z - Z_NEAR) / GROUND_CELL_SIZE)]];
    if (!image->image)
        pd->system->logToConsole("invalid frame: %i", prop->frame);
    pd->graphics->drawBitmap(image->image, (int)(x - image->w / 2), (int)(y - image->h), 0);
}

static void draw_snowball(Drawable* drawable, uint32_t* bitmap) {
    DrawableProp* prop = &drawable->prop;

    float w = 199.5f / prop->pos.z;
    float x = 199.5f + w * prop->pos.x;
    float y = 119.5f - w * prop->pos.y;
    PropImage* image = &_snowball_frames[prop->angle];
    if (!image->image) {
        pd->system->logToConsole("missing snowball image: %i", prop->angle);
    }
    pd->graphics->drawBitmap(image->image, (int)(x - image->w / 2), (int)(y - image->h), 0);
}

// push a face to the drawing list
static void push_face(GroundFace* f, Point3d cv, const float* m, const Point3d* p, int* indices, int n, const float* normals, const float shading) {
    Point3du tmp[4];

    // transform
    int outcode = 0xfffffff, is_clipped = 0;
    float min_key = FLT_MIN;
    for (int i = 0; i < n; ++i) {
        Point3du* res = &tmp[i];
        // project using active matrix
        m_x_v(m, p[indices[i]], res->v);
        res->u = normals[indices[i]];
        if (res->u >= 1.0f) res->u = 1.0f;
        int code = res->z > Z_NEAR ? OUTCODE_FAR : OUTCODE_NEAR;
        if (res->x > res->z) code |= OUTCODE_RIGHT;
        if (-res->x > res->z) code |= OUTCODE_LEFT;
        outcode &= code;
        is_clipped += code & 2;
        if (res->z > min_key) min_key = res->z;
    }

    // visible?
    if (outcode == 0) {
        Drawable* drawable = &_drawables.all[_drawables.n++];
        drawable->draw = draw_face;
        drawable->key = min_key;
        DrawableFace* face = &drawable->face;
        face->material = f->material;
        face->light = shading;
        if (is_clipped > 0) {
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

static void push_prop(const Point3d* p, int angle, int material) {
    Drawable* drawable = &_drawables.all[_drawables.n++];
    drawable->draw = draw_prop;
    drawable->key = p->z;
    drawable->prop.material = material;
    drawable->prop.pos = *p;
    drawable->prop.angle = angle;
}

static void push_coin(const Point3d* p, int time) {
    static int animation[9] = { 0,1,2,3,4,3,2,1 };
    Drawable* drawable = &_drawables.all[_drawables.n++];
    drawable->draw = draw_coin;
    drawable->key = p->z;
    drawable->coin.pos = *p;
    drawable->coin.frame = animation[time%8];
}

static void push_snowball(const Point3d* p, int angle) {
    Drawable* drawable = &_drawables.all[_drawables.n++];
    drawable->draw = draw_snowball;
    drawable->key = p->z;
    drawable->prop.material = 4;
    drawable->prop.pos = *p;
    drawable->prop.angle = angle%360;
}


int render_sky(float* m, uint32_t* bitmap) {
    // cam up in world space
    Point3d n = { .v = {-m[4],-m[5],-m[6]} };

    // intersection between camera eye and up plane(world space)
    Point3d p = { .v = { 0.f, -n.z / n.y, 1.f } };

    float w = 199.5f / p.z;
    float y0 = 119.5f - w * p.y;

    // horizon 'normal'
    n.z = 0;
    v_normz(&n);
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
    
    memset(bitmap, 0x00, h0 * LCD_ROWSIZE);
    memcpy(bitmap + h0 * LCD_ROWSIZE / sizeof(uint32_t), src, (h1-h0) * LCD_ROWSIZE);
    memset(bitmap + h1 * LCD_ROWSIZE / sizeof(uint32_t), 0xff, (LCD_ROWS - h1) * LCD_ROWSIZE);

    return angle;
}

static void collect_tiles(const Point3d pos, float base_angle) {
    float x = pos.x / GROUND_CELL_SIZE, y = pos.z / GROUND_CELL_SIZE;
    int x0 = (int)x, y0 = (int)y;

    // reset tiles
    memset((uint8_t*)_visible_tiles, 0, sizeof(uint32_t) * GROUND_SIZE);

    // current tile is always in
    _visible_tiles[y0] |= 1 << x0;

    for (int i = 0; i < RAYCAST_PRECISION; ++i) {
        float angle = base_angle + _raycast_angles[i];
        float v = cosf(angle), u = sinf(angle);

        int mapx = x0, mapy = y0;
        int mapdx = 1, mapdy = 1;
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

        for (int dist = 0; dist < 16; ++dist) {
            if (distx < disty) {
                distx += ddx;
                mapx += mapdx;
            }
            else {
                disty += ddy;
                mapy += mapdy;
            }
            // out of range?
            // if (((mapx | mapy) & 0xffffffe0) != 0) break;
            if (mapx > 31 || mapx < 0 || mapy > 31 || mapy < 0) break;

            _visible_tiles[mapy] |= 1 << mapx;
        }
    }
}

// render ground
void render_ground(Point3d cam_pos, float cam_angle, float* m, uint32_t* bitmap) {
    int angle = render_sky(m, bitmap);

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
                GroundSlice* s1 = _ground.slices[j + 1];
                const Point3d verts[4] = {
                {.v = {(float)i * GROUND_CELL_SIZE,         s0->h[i] + s0->y - y_offset,       (float)j * GROUND_CELL_SIZE}},
                {.v = {(float)(i + 1) * GROUND_CELL_SIZE,   s0->h[i + 1] + s0->y - y_offset,   (float)j * GROUND_CELL_SIZE}},
                {.v = {(float)(i + 1) * GROUND_CELL_SIZE,   s1->h[i + 1] + s1->y - y_offset,   (float)(j + 1) * GROUND_CELL_SIZE}},
                {.v = {(float)i * GROUND_CELL_SIZE,         s1->h[i] + s1->y - y_offset,       (float)(j + 1) * GROUND_CELL_SIZE}} };
                // transform
                GroundFace* f0 = &s0->faces[2 * i];
                GroundFace* f1 = f0->quad?f0:&s0->faces[2 * i + 1];
                const float normals[4] = {
                    (4.0f + s0->h[i]) / 8.f,
                    (4.0f + s0->h[i + 1]) / 8.f,
                    (4.0f + s1->h[i + 1]) / 8.f,
                    (4.0f + s1->h[i]) / 8.f
                };

                // camera to face point
                const Point3d cv = { .x = verts[0].x - cam_pos.x,.y = verts[0].y - cam_pos.y,.z = verts[0].z - cam_pos.z };
                if (f0->quad) {
                    if (v_dot(&f0->n, &cv) < 0.f)
                    {
                        push_face(f0, cv, m, verts, (int[]) { 0, 1, 2, 3 }, 4, normals, shading_band);
                    }
                }
                else {
                    if (v_dot(&f0->n, &cv) < 0.f)
                    {
                        push_face(f0, cv, m, verts, (int[]) { 0, 2, 3 }, 3, normals, shading_band);
                    }
                    if (v_dot(&f1->n, &cv) < 0.f)
                    {
                        push_face(f1, cv, m, verts, (int[]) { 0, 1, 2 }, 3, normals, shading_band);
                    }
                }
                // draw prop (if any)
                int prop_id = s0->props[i];
                if (prop_id) {
                    Point3d pos, res;
                    v_lerp(&verts[0], &verts[2], s0->prop_t[i], &pos);
                    m_x_v(m, pos, res.v);
                    if (res.z > Z_NEAR && res.z < GROUND_CELL_SIZE * 16) {
                        if (prop_id == PROP_COIN) {
                            push_coin(&res, time_offset + j + _z_offset);
                        }
                        else {
                            push_prop(&res, angle, prop_id);
                        }
                    }
                }
            }
        }
    }

    // "death" animation?
    if (_snowball.is_active) {
        Point3d res;
        m_x_v(m, _snowball.pos, res.v);
        if (res.z > Z_NEAR) {
            push_snowball(&res, _snowball.angle);
        }
    }

    // sort & renders back to front
    if (_drawables.n > 0) {
        for (int i = 0; i < _drawables.n; ++i) {
            _sortables[i] = &_drawables.all[i];
        }
        qsort(_sortables, (size_t)_drawables.n, sizeof(Drawable*), cmp_drawable);

        // rendering
        for (int k = _drawables.n - 1; k >= 0; --k) {
            _sortables[k]->draw(_sortables[k], bitmap);
        }
    }
}

