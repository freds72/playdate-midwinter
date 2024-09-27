
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
#include "particles.h"
#include "drawables.h"
#include "ground_limits.h"
#include "simd.h"
#include "rand_r.h"

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
    GroundFace f0;
    GroundFace f1;
} GroundTile;

typedef struct {
    // main track position (in tiles)
    uint8_t extents[2];
    uint16_t is_checkpoint;
    // tracks positions as bitmask
    uint32_t tracks_mask;
    float y;
    // main track extents
    float center;

    // tiles (horiz row)
    float heights[GROUND_WIDTH];
    GroundTile tiles[GROUND_WIDTH];
} GroundSlice;

// active slices + misc "globals"
typedef struct {
    // 
    int slice_id;
    // start slice index
    int plyr_z_index;
    int max_pz;
    int r_seed;
    float slice_y;
    float noise_y_offset;
    // active tracks
    Tracks* tracks;
    GroundSlice* slices[GROUND_HEIGHT];
} Ground;

#define PROP_FLAG_HITABLE   1
#define PROP_FLAG_COLLECT   2
#define PROP_FLAG_KILL      4
#define PROP_FLAG_3D        8
#define PROP_FLAG_JUMP      16
#define PROP_FLAG_Y_ROTATE  32
#define PROP_FLAG_COIN      64
#define PROP_FLAG_ROTATE_SLOW    128

typedef struct {
    int flags;
    float radius;
    ThreeDModel* model;
} PropProperties;

static PropProperties _props_properties[NEXT_PROP_ID + 1] = {0};

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
    float m[MAT4x4];
} RenderProp;

static struct {
    int n;
    RenderProp props[MAX_RENDER_PROPS];
} _render_props;

// raycasting angles
#define RAYCAST_PRECISION 256
typedef struct {
    int distance;
    float angle;
} Ray;
static Ray _rays[RAYCAST_PRECISION] = {0};

// global buffer to store slices
static GroundSlice _slices_buffer[GROUND_HEIGHT] = {0};
// active ground
static Ground _ground;

static GroundParams active_params;

// 16 32 * 4 bytes bitmaps
uint32_t _dithers[32 * 16];

// 16 32 * 8 bytes bitmaps (duplicated on x)
static uint8_t _dither_ramps[8 * 32 * 16] = {0};
static uint8_t _danger_dither_ramps[8 * 32 * 16] = {0};

// 16 32 * 4 bytes bitmaps
uint32_t _ordered_dithers[32 * 16] = {0};

static uint32_t* _backgrounds[61] = {0};
static int _backgrounds_heights[61] = {0};

// compute normal and assign material for faces
static int _z_offset = 0;
static void mesh_slice(int j) {
    _z_offset++;
    GroundSlice* s0 = _ground.slices[j];
    GroundSlice* s1 = _ground.slices[j + 1];

    // base slope normal
    Point3d sn = { .x = 0, .y = GROUND_CELL_SIZE, .z = s0->y - s1->y };
    v_normz(&sn);

    for (int i = 0; i < GROUND_WIDTH - 1; i++) {
        const float y0 = s0->heights[i] + s0->y;
        // v1-v0
        const Point3d u1 = { .v = {(float)GROUND_CELL_SIZE, s0->heights[i + 1] + s0->y - y0, 0.f} };
        // v2-v0
        const Point3d u2 = { .v = {(float)GROUND_CELL_SIZE, s1->heights[i + 1] + s1->y - y0, (float)GROUND_CELL_SIZE} };
        // v3-v0
        const Point3d u3 = { .v = {0.f, s1->heights[i] + s1->y - y0, (float)GROUND_CELL_SIZE} };

        GroundFace* f0 = &s0->tiles[i].f0;
        GroundFace* f1 = &s0->tiles[i].f1;
        v_cross(u3, u2, &f0->n);
        v_cross(u2, u1, &f1->n);
        v_normz(&f0->n);
        v_normz(&f1->n);
        f0->flags = f0->n.y < 0.75f ? GROUNDFACE_FLAG_ROCK : GROUNDFACE_FLAG_SNOW;
        if (v_dot(f0->n, f1->n) < 0.999f) {
            f1->flags = f1->n.y < 0.75f ? GROUNDFACE_FLAG_ROCK : GROUNDFACE_FLAG_SNOW;;
        }
        else {
            f0->flags |= GROUNDFACE_FLAG_QUAD;
        }
    }
}

static void make_slice(GroundSlice* slice, float y) {
    static int trees[] = { PROP_TREE0,PROP_TREE0,PROP_TREE1,PROP_TREE1,PROP_TREE2,PROP_TREE3,PROP_TREE3,PROP_TREE4,PROP_TREE4,PROP_TREE4,PROP_TREE5,PROP_TREE5,PROP_TREE5,PROP_LOG };
    static int num_trees = sizeof(trees) / sizeof(PROP_TREE0);

    // smooth altitude changes
    _ground.slice_y = lerpf(_ground.slice_y, y, 0.2f);
    y = _ground.slice_y;
    // capture height
    slice->y = y;
    slice->tracks_mask = 0;

    update_tracks();

    // generate height
    uint32_t shadow_mask = 0;
    for (int i = 0; i < GROUND_WIDTH; ++i) {
        slice->heights[i] = (perlin2d((16.f * i) / GROUND_WIDTH, _ground.noise_y_offset, 0.25f, 4)) * 4.f * active_params.slope;
        
    }
    for (int i = 0; i < GROUND_WIDTH; ++i) {
        slice->tiles[i].prop_id = 0;
    }

    _ground.noise_y_offset += (active_params.slope + randf_seeded()) / 4.f;

    int imin = 3, imax = GROUND_WIDTH - 3;
    float main_track_x = GROUND_CELL_SIZE * (imin + imax) / 2.f;
    int is_checkpoint = 0;
    Tracks* tracks = _ground.tracks;    
    for (int k = 0; k < tracks->n; ++k) {
        Track* t = &tracks->tracks[k];
        if (!t->is_dead) {
            // center on track
            const int i0 = t->imin, i1 = t->imax + 1;
            if (t->is_main) {
                // random filling
                for (int i = 3; i < i0 - 1; i++) {
                    GroundTile* tile = &slice->tiles[i];
                    if (randf_seeded() > active_params.props_rate) {
                        tile->prop_id = trees[randi_seeded(num_trees)];
                        tile->prop_t = randf_seeded();
                        // slight shading under trees
                        shadow_mask |= 0x3 << i;
                    }
                }

                for (int i = i1 + 1; i < GROUND_WIDTH - 2; i++) {
                    GroundTile* tile = &slice->tiles[i];
                    if (randf_seeded() > active_params.props_rate) {
                        tile->prop_id = trees[randi_seeded(num_trees)];
                        tile->prop_t = randf_seeded();
                        // slight shading under trees
                        shadow_mask |= 0x3 << i;
                    }
                }

                main_track_x = t->x;
                imin = i0;
                imax = i1;  
                
                for (int i = i0; i < i1; ++i) {
                    // remove props from track
                    int prop_id = 0;
                    float prop_t = 0.5f;
                    switch (_ground.tracks->pattern[i]) {
                    case 'W': prop_id = PROP_WARNING; break;
                    case 'R': prop_id = PROP_ROCK; break;
                    case 'P': prop_id = PROP_SNOWPLOW; break;
                    case 'M': prop_id = PROP_COW; prop_t = randf_seeded(); break;
                    case 'J': prop_id = PROP_JUMPPAD; break;
                    case 'S': prop_id = PROP_START; break;
                    case 'D': prop_id = PROP_GORIGHT; break;
                    case 'G': prop_id = PROP_GOLEFT; break;
                    case 'C': prop_id = PROP_COIN; break;
                        // hole
                    case 'O': slice->heights[i] = -4.f; break;
                        // geump :)
                    case '1': slice->heights[i] += 0.75f * active_params.slope; break;
                    case '2': slice->heights[i] += 1.f * active_params.slope; break;
                    case '3': slice->heights[i] += 1.5f * active_params.slope; break;
                    case '4': slice->heights[i] += 2.5f * active_params.slope; break;
                    case '5': slice->heights[i] += 3.5f * active_params.slope; break;
                    case 'T':
                        prop_id = trees[randi_seeded(num_trees)];
                        prop_t = randf_seeded();
                        // tree shading
                        shadow_mask |= 0x3 << i;
                        break;
                    default:
                        ;
                    }

                    slice->tiles[i].prop_id = prop_id;
                    slice->tiles[i].prop_t = prop_t;
                    slice->tracks_mask |= 1 << i;
                    slice->is_checkpoint = i % 8 == 0;
                }
            }
            else {
                // side tracks are less obvious
                for (int i = i0; i < i1; ++i) {
                    slice->tracks_mask |= 1 << i;
                }
            }
        }
    }

    slice->center = main_track_x;
    slice->extents[0] = imin;
    slice->extents[1] = imax;

    slice->is_checkpoint = is_checkpoint;

    // side walls
    slice->heights[0] = 15.f + 5.f * randf_seeded();
    slice->heights[GROUND_WIDTH - 1] = 15.f + 5.f * randf_seeded();
    if (active_params.tight_mode) {
        for (int i = 1; i < imin - 1; i++) {
            slice->heights[i] = slice->heights[0];
            slice->tiles[i].prop_id = 0;
        }
        for (int i = GROUND_WIDTH - 2; i > imax + 1; i--) {
            slice->heights[i] = slice->heights[GROUND_WIDTH - 1];
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
        imax = GROUND_WIDTH - 2;
    }

    // apply props shadows
    slice->tracks_mask |= shadow_mask;
    //  + nice transition
    slice->heights[imin] = lerpf(slice->heights[imin - 1], slice->heights[imin], lerpf(0.666f,0.8f,randf_seeded()));
    slice->heights[imax] = lerpf(slice->heights[imax + 1], slice->heights[imax], lerpf(0.666f,0.8f,randf_seeded()));

    _ground.slice_id++;

    /*
    char buffer[33];
    memset(buffer, 32, 32);
    buffer[32] = 0;
    for (int i = 0; i < 32; i++) {        
        buffer[i] = '0' + slice->tiles[i].prop_id;
    }
    pd->system->logToConsole("%s [%i %i]", _ground.tracks->pattern, imin, imax);
    */
}

void make_ground(GroundParams params, TrackPatterns* patterns) {
    active_params = params;
    
    // init random seed
    rand_r_init(params.r_seed);

    // reset global params
    _ground.slice_id = 0;
    _ground.slice_y = 0;
    _ground.noise_y_offset = 16.f * randf_seeded();
    _ground.plyr_z_index = (GROUND_HEIGHT / 2) - 1;
    _ground.max_pz = 0;


    // init track generator
    make_tracks(4 * GROUND_CELL_SIZE, (GROUND_WIDTH - 5)*GROUND_CELL_SIZE, params, &_ground.tracks);

    for (int i = 0; i < GROUND_HEIGHT; ++i) {
        // reset slices
        _ground.slices[i] = &_slices_buffer[i];
        make_slice(_ground.slices[i], -i * params.slope);
        memcpy(patterns->pattern[i],_ground.tracks->pattern, TRACK_PATTERN_WIDTH);
    }
    for (int i = 0; i < GROUND_HEIGHT - 1; ++i) {
        mesh_slice(i);
    }
}

void update_ground(const Point3d p, int* slice_id, TrackPattern* pattern, Point3d* offset) {
    // TODO: FIX!!!
    // prevent going up slope!
    float pz = p.z;
    if (pz < 8 * GROUND_CELL_SIZE) pz = 8 * GROUND_CELL_SIZE;
    *offset = (Point3d){ .v = {0} };

    // handles faster than 1 tile moves
    while ( pz/ GROUND_CELL_SIZE > _ground.plyr_z_index ) {
        // shift back
        pz -= GROUND_CELL_SIZE;
        offset->z -= GROUND_CELL_SIZE;
        _ground.max_pz -= GROUND_CELL_SIZE;
        GroundSlice* old_slice = _ground.slices[0];
        const float old_y = old_slice->y;
        offset->y -= old_y;
        // drop slice 0
        for (int i = 1; i < GROUND_HEIGHT; ++i) {
            _ground.slices[i - 1] = _ground.slices[i];
            _ground.slices[i - 1]->y -= old_y;
        }
        // move shifted slices back to top
        _ground.slices[GROUND_HEIGHT - 1] = old_slice;        

        // use previous baseline
        make_slice(old_slice, _ground.slices[GROUND_HEIGHT - 2]->y - active_params.slope * (randf_seeded() + 0.5f));
        mesh_slice(GROUND_HEIGHT - 2);
    }
    // update y offset
    if (pz > _ground.max_pz) {
        _ground.max_pz = (int)pz;
    }

    // update particles (inc. world shifting)
    update_particles(*offset);

    *slice_id = _ground.slice_id;
    memcpy(pattern,_ground.tracks->pattern, TRACK_PATTERN_WIDTH);
}

void get_start_pos(Point3d* out) {
    *out = (Point3d){.v = {
        _ground.slices[_ground.plyr_z_index]->center, 
        0,
        (float)_ground.plyr_z_index * GROUND_CELL_SIZE} };
    Point3d n;
    float y;
    get_face(*out, &n, &y);
    out->y = y;
}

int get_face(const Point3d pos, Point3d* nout, float* yout) {
    // z slice
    int i = (int)(pos.x / GROUND_CELL_SIZE), j = (int)(pos.z / GROUND_CELL_SIZE);
    // outside ground?
    if (i < 0 || i >= GROUND_WIDTH || j < 0 || j >= GROUND_HEIGHT) {
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
    make_v((Point3d) { .v = {(float)i * GROUND_CELL_SIZE, s0->heights[i] + s0->y, (float)j * GROUND_CELL_SIZE} }, pos, & ptOnFace);
         
    // height
    *yout = pos.y - v_dot(ptOnFace, f->n) / f->n.y;
    // face normal
    nout->v[0] = f->n.v[0];
    nout->v[1] = f->n.v[1];
    nout->v[2] = f->n.v[2];

    return 1;
}

// get slice extents
void get_track_info(const Point3d pos, float* xmin, float* xmax, float* z, int* checkpoint, float* angleout) {
    int j = (int)(pos.z / GROUND_CELL_SIZE);
    if (j < 0 || j >= GROUND_HEIGHT) {
        pd->system->error("Invalid z position: %f", pos.z);
        return;
    }

    const GroundSlice* s0 = _ground.slices[j];
    *xmin = (float)(s0->extents[0] * GROUND_CELL_SIZE);
    *xmax = (float)(s0->extents[1] * GROUND_CELL_SIZE);
    // activate checkpoint at middle of cell
    *z = (j + 0.5f) * GROUND_CELL_SIZE;
    *checkpoint = s0->is_checkpoint;


    // find nearest checkpoint
    float x = _ground.slices[j + 2]->center, y = 2;
    for (int k = j + 2; k < j + 10 && k < GROUND_HEIGHT; ++k) {
        GroundSlice* s = _ground.slices[k];
        if (s->is_checkpoint) {
            x = s->center;
            y = (float)(k - j);
            break;
        }
    }
    // direction to track ahead (rebase to half circle)
    *angleout = 0.5f * atan2f(y * GROUND_CELL_SIZE, x - pos.x) / PI - 0.25f;
}

void get_props(Point3d pos, PropInfo** info, int* nout) {
    const int ii = (int)(pos.x / GROUND_CELL_SIZE);
    int i0 = ii - 4, i1 = ii + 4;
    if (i0 < 0) i0 = 0;
    if (i1 > GROUND_WIDTH) i1 = GROUND_WIDTH;

    const int j0 = (int)(pos.z / GROUND_CELL_SIZE) + 1;
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
                    (Point3d){ .v = {(float)i * GROUND_CELL_SIZE,         s0->heights[i] + s0->y,       (float)j * GROUND_CELL_SIZE} },
                    (Point3d){ .v = {(float)(i + 1) * GROUND_CELL_SIZE,   s1->heights[i + 1] + s1->y,   (float)(j + 1) * GROUND_CELL_SIZE } },
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
void clear_checkpoint(const Point3d pos) {
    int j = (int)(pos.z / GROUND_CELL_SIZE);
    GroundSlice* s0 = _ground.slices[j];
    s0->is_checkpoint = 0;
}

void collide(const Point3d pos, float radius, int* hit_type)
{
    // z slice
    int i0 = (int)(pos.x / GROUND_CELL_SIZE), j0 = (int)(pos.z / GROUND_CELL_SIZE);

    // default
    *hit_type = 0;

    // out of track
    if (i0 <= 0 || i0 >= GROUND_WIDTH - 2) {
        *hit_type = 2;
        return;
    }

    // square radius
    radius *= radius;

    // check all 9 cells(overkill but faster)
    float tilez = (float)(j0 - 1) * GROUND_CELL_SIZE;
    for (int j = j0 - 1; j < j0 + 2; j++, tilez += GROUND_CELL_SIZE) {
        if (j >= 0 && j < GROUND_HEIGHT) {
            GroundSlice* s0 = _ground.slices[j];
            float tilex = (float)(i0 - 1) * GROUND_CELL_SIZE;
            for (int i = i0 - 1; i < i0 + 2; i++, tilex += GROUND_CELL_SIZE) {
                if (i >= 0 && i < GROUND_WIDTH) {
                    GroundTile* t0 = &s0->tiles[i];
                    const int id = t0->prop_id;
                    // collidable actor ?
                    if (id) {
                        PropProperties* props = &_props_properties[id - 1];
                        if (props->flags & PROP_FLAG_HITABLE) {

                            // generate vertex
                            Point3d v0 = (Point3d){ .v = {(float)tilex,s0->heights[i] + s0->y,(float)tilez}};
                            Point3d v2 = (Point3d){ .v = {(float)(tilex + GROUND_CELL_SIZE),s0->heights[i + 1] + s0->y,(float)(tilez + GROUND_CELL_SIZE)} };
                            Point3d res;
                            v_lerp(v0, v2, t0->prop_t, &res);
                            make_v(pos, res, &res);
                            if (res.x * res.x + res.z * res.z < radius + props->radius * props->radius) {
                                if (props->flags & PROP_FLAG_COIN) {
                                    *hit_type = 3;
                                }
                                else if (props->flags & PROP_FLAG_KILL) {
                                    // insta-kill?
                                    *hit_type = 2;
                                }
                                else if (props->flags & PROP_FLAG_JUMP) {
                                    *hit_type = 4;
                                }
                                else {
                                    *hit_type = 1;
                                }

                                if (props->flags & PROP_FLAG_COLLECT) {
                                    // "collect" prop
                                    t0->prop_id = 0;
                                }

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

    // organized by ramps to have darker variant as a single line
    for (int j = 0; j < 32; ++j) {
        for (int k = 0; k < 4; k++, data++) {
            // interleaved values (4 bytes)
            _dither_ramps[i * 8 + j * 16 * 8 + k] = *data;
            _dither_ramps[i * 8 + j * 16 * 8 + k + 4] = *data;
        }
    }
}

static void load_ordered_noise(void* ptr, const int i, const int _) {
    LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, i);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (w != 32 || h != 32)
        pd->system->logToConsole("Invalid noise image format: %dx%d", w, h);

    // copy "regular" dither
    memcpy((uint8_t*)(_ordered_dithers + i * 32), data, 32 * sizeof(uint32_t));
}


static void load_danger_noise(void* ptr, const int i, const int _) {
    LCDBitmap* bitmap = pd->graphics->getTableBitmap((LCDBitmapTable*)ptr, i);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (w != 32 || h != 32)
        pd->system->logToConsole("Invalid noise image format: %dx%d", w, h);

    // write danger pattern
    uint32_t pattern = ~0x83838383;
    uint32_t* data32 = (uint32_t*)data;
    for (int i = 0; i < 32; i++) {
        data32[i] &= pattern;
        pattern = (pattern >> 1) | (pattern << 31);
    }

    // organized by ramps to have darker variant as a single line
    for (int j = 0; j < 32; ++j) {
        for (int k = 0; k < 4; k++, data++) {
            // interleaved values (4 bytes)
            _danger_dither_ramps[i * 8 + j * 16 * 8 + k] = *data;
            _danger_dither_ramps[i * 8 + j * 16 * 8 + k + 4] = *data;
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

    // read dither table
    pd->system->formatString(&path, "images/generated/noise32x32");
    pd->system->logToConsole("Loading blue noise: %s", path);
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

    for (int i = 0; i < 16; ++i) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_danger_noise,
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

    // read ordered dither table
    pd->system->formatString(&path, "images/generated/bayer-noise32x32");
    pd->system->logToConsole("Loading Bayer noise: %s", path);
    bitmaps = pd->graphics->loadBitmapTable(path, &err);

    if (!bitmaps)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);

    for (int i = 0; i < 16; ++i) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_ordered_noise,
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
        // 1.5f to overshoot 
        float t = 1.5f * (((float)i) / RAYCAST_PRECISION - 0.5f);
        const float angle = atan2f(MAX_TILE_DIST, MAX_TILE_DIST * t * 2.f) - PI / 2.f;
        // using a 24:8 "fixed" value
        const int dist_max = (int)((MAX_TILE_DIST << 8) / cosf(angle));
        _rays[i] = (Ray){
            .distance = dist_max * dist_max,
            .angle = angle
        };            
    }

    // props config (todo: get from lua?)

    // forest stuff
    _props_properties[PROP_TREE0 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f};
    _props_properties[PROP_TREE1 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    _props_properties[PROP_TREE2 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.f };
    _props_properties[PROP_TREE3 - 1] = (PropProperties){ .flags = 0, .radius = 1.8f };
    _props_properties[PROP_TREE4 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    _props_properties[PROP_TREE5 - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    _props_properties[PROP_LOG - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.8f };
    // checkpoint flags
    _props_properties[PROP_CHECKPOINT_LEFT - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    _props_properties[PROP_CHECKPOINT_RIGHT - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    _props_properties[PROP_START - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE, .radius = 1.f };
    // obstacles
    _props_properties[PROP_ROCK - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_KILL, .radius = 3.f };
    _props_properties[PROP_COW - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_KILL, .radius = 2.5f };
    _props_properties[PROP_SNOWPLOW - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_KILL, .radius = 3.f };

    // snowball
    _props_properties[PROP_SNOWBALL - 1] = (PropProperties){ .flags = PROP_FLAG_KILL, .radius = 2.f };
    _props_properties[PROP_SPLASH - 1] = (PropProperties){ .flags = 0, .radius = 0.f };
    // 
    _props_properties[PROP_JUMPPAD - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_JUMP | PROP_FLAG_Y_ROTATE | PROP_FLAG_COLLECT, .radius = 2.0f };
    // coin
    _props_properties[PROP_COIN - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_COLLECT | PROP_FLAG_COIN | PROP_FLAG_Y_ROTATE, .radius = 2.0f };

    _props_properties[PROP_INVERT - 1] = (PropProperties){ .flags = PROP_FLAG_HITABLE | PROP_FLAG_COLLECT | PROP_FLAG_Y_ROTATE, .radius = 2.0f };

    _props_properties[PROP_EAGLES - 1] = (PropProperties){ .flags = PROP_FLAG_Y_ROTATE | PROP_FLAG_ROTATE_SLOW};

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

// cache entry (transformed point in camera space)
typedef struct {
    Point3du p;
    int outcode;
} CameraPoint;

typedef struct {
    uint16_t i;
    uint16_t j;
    uint32_t mask;
    GroundSlice* slice;
    CameraPoint* cache;
} GroundSliceCoord;

// clip polygon against near-z
static int z_poly_clip(const float z, const float flip, Point3du* in, int n, Point3du* out) {
    Point3du v0 = in[n - 1];
    float d0 = flip * (v0.z - z);
    int nout = 0;
    for (int i = 0; i < n; i++) {
        Point3du v1 = in[i];
        int side = d0 > 0;
        if (side) out[nout++] = (Point3du){ .v = { v0.x, v0.y, v0.z }, .u = v0.u, .light = v0.light };
        const float d1 = flip * (v1.z - z);
        if ((d1 > 0) != side) {
            // clip!
            const float t = d0 / (d0 - d1);
            out[nout++] = (Point3du){ .v = {
                lerpf(v0.x,v1.x,t),
                lerpf(v0.y,v1.y,t),
                z},
                .u = lerpf(v0.u,v1.u,t),
                .light = lerpf(v0.light,v1.light,t)
            };
        }
        v0 = v1;
        d0 = d1;
    }
    return nout;
}

static void draw_tile(Drawable* drawable, uint8_t* bitmap) {
    DrawableFace* face = &drawable->face;

    const int n = face->n;
    Point3du* pts = face->pts;
    for (int i = 0; i < n; i++) {
        // project 
        const float w = 1.f / pts[i].z;
        pts[i].x = 199.5f + 199.5f * w * pts[i].x;
        pts[i].y = 119.5f - 199.5f * w * pts[i].y;

        // works ok
        float shading = face->material == GROUNDFACE_FLAG_SNOW ? 4.0f * pts[i].u + 8.f * w : 6.0f + 4.0f * pts[i].u + 4.f * w;
        // attenuation
        shading *= pts[i].light;
        if (shading > 15.f) shading = 15.f;
        if (shading < 0.f) shading = 0.f;

        float dist = Z_FAR - pts[i].z - 2*GROUND_CELL_SIZE;
        float dist_shading = dist / (2.0f * GROUND_CELL_SIZE);
        if (dist_shading > 1.f) dist_shading = 1.f;
        if (dist_shading < 0.f) dist_shading = 0.f;

        pts[i].u = shading * dist_shading;
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
}

static void draw_blinking_tile(Drawable* drawable, uint8_t* bitmap) {
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
    texfill(pts, n, _danger_dither_ramps, bitmap);    
}

static void draw_face(Drawable* drawable, uint8_t* bitmap) {
    DrawableFace* face = &drawable->face;

    const int n = face->n;
    Point3du* pts = face->pts;
    for (int i = 0; i < n; ++i) {
        // project 
        const float w = 199.5f / pts[i].z;
        pts[i].x = 199.5f +  w * pts[i].x;
        pts[i].y = 119.5f -  w * pts[i].y;
    }

    // 
    const float dist = drawable->key - (MAX_TILE_DIST * 0.707f - 2.f) * GROUND_CELL_SIZE;
    if (!(face->flags & FACE_FLAG_TRANSPARENT)) {
        float shading = dist / (2.f * GROUND_CELL_SIZE);
        if (shading > 1.f) shading = 1.f;
        if (shading < 0.f) shading = 0.f;
        polyfill(pts, n, _ordered_dithers + (int)(face->material * (1.f - shading)) * 32, (uint32_t*)bitmap);
    }

    // don't "pop" edges if too far away
    if ((face->flags & FACE_FLAG_EDGES) && dist < 0.f) {
        Point3du* p0 = &pts[n - 1];
        for (int i = 0; i < n; ++i) {
            Point3du* p1 = &pts[i];
            if (p0->u) {
                pd->graphics->drawLine((int)p0->x, (int)p0->y, (int)p1->x, (int)p1->y, 1, kColorBlack);
            }
            p0 = p1;
        }
    }
}


// push a face to the drawing list
static void push_tile(const GroundFace* f, const Mat4 m, GroundSliceCoord* coords, int n, const float light, const int is_danger) {
    Point3du tmp[4];

    // transform
    int outcode = 0xfffffff, is_clipped_near = 0;
    float min_key = -FLT_MAX;
    for (int i = 0; i < n; ++i) {
        // check cache entry
        GroundSliceCoord c = coords[i];
        // grab correct point cache line
        CameraPoint* cp = &c.cache[c.i];
        // not fresh?
        if (cp->outcode == -1) {
            Point3du* res = &cp->p;
            // compute point  
            const float h = c.slice->heights[c.i];
            // project using active matrix
            m_x_v(m, (Point3d) { .v = {(float)(c.i << 2), h + c.slice->y, (float)(c.j << 2)} }, (Point3d*)res);
            
            // TODO: check if useful vs. collect tiles
            const int code =
                (((Flint) { .f = Z_FAR - res->z  }.i >> 31) & OUTCODE_FAR) |
                (((Flint) { .f = res->z - Z_NEAR }.i >> 30) & OUTCODE_NEAR) |
                (((Flint) { .f = res->z - res->x }.i >> 29) & OUTCODE_RIGHT) |
                (((Flint) { .f = res->z + res->x }.i >> 28) & OUTCODE_LEFT);

            cp->outcode = code;

            // light
            res->u = (4.0f + h) / 8.f;
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
        Drawable* drawable = pop_drawable(min_key);
        drawable->draw = is_danger ?draw_blinking_tile: draw_tile;
        drawable->key = min_key;
        DrawableFace* face = &drawable->face;
        face->material = f->flags & GROUNDFACE_FLAG_MATERIAL_MASK;
        if (is_clipped_near & OUTCODE_NEAR) {
            face->n = z_poly_clip(Z_NEAR, 1.0f, tmp, n, face->pts);
        }
        else if (is_clipped_near & OUTCODE_FAR) {
            face->n = z_poly_clip(Z_FAR, -1.f, tmp, n, face->pts);
        }
        else {
            face->n = n;
            memcpy(face->pts, tmp, n * sizeof(Point3du));
        }
    }
}

void add_render_prop(const int id, const Mat4 m) {
    RenderProp* p = &_render_props.props[_render_props.n++];
    if (_render_props.n>=MAX_RENDER_PROPS)
        pd->system->error("Too many render props: %i/%i", _render_props.n, MAX_RENDER_PROPS);

    p->id = id;
    // todo: check mis-align
    memcpy(p->m, m, MAT4x4 * sizeof(float));
}

static void push_threeD_model(const int prop_id, const Point3d cv, const Mat4 m) {
    Point3du tmp[4];
    ThreeDModel* model = _props_properties[prop_id - 1].model;
    for (int j = 0; j < model->face_count; ++j) {
        ThreeDFace* f = &model->faces[j];
        // visible?
        if ( v_dot(f->n, cv) > f->cp) {
            // vert count
            int n = f->flags & FACE_FLAG_QUAD?4:3;
            // transform
            int outcode = 0xfffffff, is_clipped_near = 0;
            float min_key = FLT_MAX;
            float max_key = -FLT_MAX;
            for (int i = 0; i < n; ++i) {
                Point3du* res = &tmp[i];
                // project using active matrix
                m_x_v(m, f->vertices[i], &res->p);

                int code =
                    (((Flint) { .f = res->z - Z_NEAR }.i >> 30) & OUTCODE_NEAR) |
                    (((Flint) { .f = res->z - res->x }.i >> 29) & OUTCODE_RIGHT) |
                    (((Flint) { .f = res->z + res->x }.i >> 28) & OUTCODE_LEFT);

                if (res->z < min_key) min_key = res->z;
                if (res->z > max_key) max_key = res->z;
                outcode &= code;
                is_clipped_near |= code;
                // use u to mark sharp edges
                res->u = f->edges & (1 << i);
            }

            // visible?
            if (outcode == 0) {
                const float sortkey = f->flags & FACE_FLAG_LARGE ? max_key : min_key;
                Drawable* drawable = pop_drawable(sortkey);
                drawable->draw = draw_face;
                drawable->key = sortkey;
                DrawableFace* face = &drawable->face;
                face->flags = f->flags;
                face->material = f->material;
                if (is_clipped_near & OUTCODE_NEAR) {
                    face->n = z_poly_clip(Z_NEAR, 1.0f, tmp, n, face->pts);
                }
                else {
                    face->n = n;
                    memcpy(face->pts, tmp, n * sizeof(Point3du));
                }
            }       
        }
    }
}

static void push_and_transform_threeD_model(const int prop_id, const Point3d cam_pos, const Mat4 cam_m, const Mat4 m) {
    Point3d inv_cam_pos;
    Mat4 mvv;
    const int flags = _props_properties[prop_id - 1].flags;
    if (flags & PROP_FLAG_Y_ROTATE) {
        // disregard model matrix (beside position)
        Mat4 tmp = {
            1.f,0.f,0.f,0.f,
            0.f,1.f,0.f,0.f,
            0.f,0.f,1.f,0.f,
            m[12],m[13],m[14],1.f};
        // shift rotation to make it more natural
        const float rot_scale = flags & PROP_FLAG_ROTATE_SLOW ? 1.0f : 2.0f;
        m_x_y_rot(tmp, rot_scale * pd->system->getElapsedTime() + m[12], mvv);
        m_inv_x_v(mvv, cam_pos, &inv_cam_pos);
        m_x_m(cam_m, mvv, tmp);
        push_threeD_model(prop_id, inv_cam_pos, tmp);
    }
    else {
        m_x_m(cam_m, m, mvv);

        // cam pos in 3d model space
        m_inv_x_v(m, cam_pos, &inv_cam_pos);

        push_threeD_model(prop_id, inv_cam_pos, mvv);
    }
}

int render_sky(const Mat4 m, uint8_t* screen) {
    uint32_t* bitmap = (uint32_t*)screen;

    // cam up in world space
    Point3d n = { .v = {-m[4],-m[5],-m[6]} };

    // intersection between camera eye and up plane(world space)
    Point3d p = { .v = { 0.f, -n.z / n.y, 1.f } };

    float w = 199.5f / p.z;
    float y0 = 109.5f - w * p.y;

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
    
    memset(bitmap, 0xff, h0 * LCD_ROWSIZE);
    const uint32_t *dither_base = _ordered_dithers + 8 * 32;
    bitmap += h0 * LCD_ROWSIZE / sizeof(uint32_t);
    for (int h = h0; h < h1; ++h) {
        const uint32_t dither = dither_base[h & 31];
        for (int i = 0; i < LCD_ROWSIZE / sizeof(uint32_t); ++i, ++src, ++bitmap) {
            *bitmap = (*src & dither) | (~(*src));
        }
    }
    memset(bitmap, 0xff, (LCD_ROWS - h1) * LCD_ROWSIZE);

    return angle;
}

static void collect_tiles(uint32_t visible_tiles[GROUND_HEIGHT], const Point3d pos, float base_angle) {
    float x = pos.x / GROUND_CELL_SIZE, y = pos.z / GROUND_CELL_SIZE;
    int x0 = (int)x, y0 = (int)y;

    // current tile is always in
    visible_tiles[y0] |= 1 << x0;

    for (int i = 0; i < RAYCAST_PRECISION; ++i) {
        float angle = base_angle + _rays[i].angle;
        float v = cosf(angle), u = sinf(angle);

        int mapx = x0, mapy = y0;
        int mapdx = 1, mapdy = 1;
        // convert to fixed 24:8
        int ddx = (int)((1 << 8) / u), ddy = (int)((1 << 8) / v);
        int distx = 0, disty = 0;
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

        const int dist_max = _rays[i].distance;
        while((distx * distx) + (disty * disty) < dist_max) {
            if (distx < disty) {
                distx += ddx;
                mapx += mapdx;
            }
            else {
                disty += ddy;
                mapy += mapdy;
            }
            // out of range?
            if (mapx&~(GROUND_WIDTH-1) || mapy<0 || mapy >= GROUND_HEIGHT) break;

            visible_tiles[mapy] |= 1 << mapx;
        }
    }
}

// render ground

void render_ground(const Point3d cam_pos, const float cam_tau_angle, const Mat4 m, uint32_t blink, uint8_t * bitmap) {
    // cache lines
    CameraPoint c0[GROUND_WIDTH];
    CameraPoint c1[GROUND_WIDTH];

    CameraPoint* cache[2] = { c0, c1 };
    // reset cache
    for (int i = 0; i < GROUND_WIDTH; ++i) {
        c0[i].outcode = -1;
        c1[i].outcode = -1;
    }

    const float cam_angle = cam_tau_angle * 2.f * PI;
    render_sky(m, bitmap);

    // collect visible tiles
    // visible tiles encoded as 1 bit per cell
    uint32_t tiles[GROUND_HEIGHT] = { 0 };
    collect_tiles(tiles, cam_pos, cam_angle);

    // transform
    reset_drawables();
    float tilez = 0.f;
    for (int j = 0; j < GROUND_HEIGHT - 1; j++, tilez += GROUND_CELL_SIZE) {
        // const uint32_t visible_tiles = tiles[j];
        const uint32_t visible_tiles = tiles[j];
        // slightly alter shading of even/odd slices
        const float shading_band = SHADING_CONTRAST * (0.5f + 0.5f * ((j + _z_offset) & 1));
        float tilex = 0.f;
        for (int i = 0; i < GROUND_WIDTH; i++, tilex+=GROUND_CELL_SIZE) {
            // is the tile bit enabled?
            if (visible_tiles & (1 << i)) {
                GroundSlice* s0 = _ground.slices[j];
                GroundTile* t0 = &s0->tiles[i];
                GroundSlice* s1 = _ground.slices[j + 1];
                const float h0 = s0->heights[i] + s0->y;
                // camera to face point
                const Point3d cv = { .x = tilex - cam_pos.x, .y = h0 - cam_pos.y, .z = tilez - cam_pos.z };
                const GroundFace* f0 = &t0->f0;
                const int is_danger = blink & (1 << i);
                if (f0->flags & GROUNDFACE_FLAG_QUAD) {
                    if (v_dot(f0->n, cv) < 0.f)
                    {
                        push_tile(f0, m, (GroundSliceCoord[]) {
                            { .slice = s0, .i = i,     .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .slice = s0, .i = i + 1, .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .slice = s1, .i = i + 1, .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask },
                            { .slice = s1, .i = i,     .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 4, shading_band, is_danger);
                    }
                }
                else {
                    if (v_dot(f0->n, cv) < 0.f)
                    {
                        push_tile(f0, m, (GroundSliceCoord[]) {
                            { .slice = s0, .i = i,      .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .slice = s1, .i = i + 1,  .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask },
                            { .slice = s1, .i = i,      .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 3, shading_band, is_danger);
                    }
                    const GroundFace* f1 = &t0->f1;
                    if (v_dot(f1->n, cv) < 0.f)
                    {
                        push_tile(f1, m, (GroundSliceCoord[]) {
                            { .slice = s0, .i = i,     .j = j,     .cache = cache[0], .mask = s1->tracks_mask},
                            { .slice = s0, .i = i + 1, .j = j,     .cache = cache[0], .mask = s1->tracks_mask },
                            { .slice = s1, .i = i + 1, .j = j + 1, .cache = cache[1], .mask = s0->tracks_mask }
                        }, 3, shading_band, is_danger);
                    }
                }
                // draw prop (if any)
                int prop_id = t0->prop_id;
                if (prop_id) {
                    const float t = t0->prop_t;
                    const Point3d pos = {
                        .x = tilex + GROUND_CELL_SIZE * t,
                        .y = h0 + (s1->heights[i + 1] + s1->y - s0->heights[i] - s0->y) * t,
                        .z = tilez + GROUND_CELL_SIZE * t
                    };
                    Point3d res;
                    m_x_v(m, pos, &res);
                    if (res.z > Z_NEAR && res.z < (float)(GROUND_CELL_SIZE * MAX_TILE_DIST)) {
                        Point3d cv;
                        Mat4 mmvm;
                        // adjust matrix to project into position
                        const int flags = _props_properties[prop_id - 1].flags;
                        if (flags & PROP_FLAG_Y_ROTATE) {
                            Mat4 tmp = {
                                1.f,0.f,0.f,0.f,
                                0.f,1.f,0.f,0.f,
                                0.f,0.f,1.f,0.f,
                                pos.x,pos.y,pos.z,1.f };
                            const float rot_scale = flags & PROP_FLAG_ROTATE_SLOW ? 1.0f : 2.0f;
                            m_x_y_rot(tmp, rot_scale * pd->system->getElapsedTime() + pos.x, mmvm);
                            m_inv_x_v(mmvm, cam_pos, &cv);
                            m_x_m(m, mmvm, tmp);
                            push_threeD_model(prop_id, cv, tmp);
                        }
                        else {
                            cv = (Point3d){.x = cam_pos.x - pos.x, .y = cam_pos.y - pos.y, .z = cam_pos.z - pos.z};
                            m_x_translate(m, pos, mmvm);
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
        for (int i = 0; i < GROUND_WIDTH; ++i) {
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

    // particles?
    push_particles(cam_pos, m);

    // sort & renders back to front
    draw_drawables(bitmap);

    /*
    uint32_t* dst = (uint32_t*)bitmap;
    for (int i = 0; i < 32; i++,dst+=LCD_ROWSIZE/sizeof(uint32_t)) {
        *dst = swap(tiles[i]);
    }
    */

    // dither gradient
    /*
    for (int j = 0; j < 32; j++) {
        uint8_t* dst = bitmap + j * LCD_ROWSIZE;
        uint8_t* src = _dither_ramps + j*16*8;
        for (int i = 0; i < 16; i++) {
            *dst++ = *src++;
            *dst++ = *src++;
            src += 6;
        }
    }
    */
}

// render "free" props without ground (for title screen say)
void render_props(const Point3d cam_pos, const Mat4 m, uint8_t* bitmap) {
    // "free" props?
    reset_drawables();
    for (int i = 0; i < _render_props.n; ++i) {
        RenderProp* prop = &_render_props.props[i];
        push_and_transform_threeD_model(prop->id, cam_pos, m, prop->m);
    }
    // reset "free" props
    _render_props.n = 0;

    // particles?
    push_particles(cam_pos, m);

    // sort & renders back to front
    draw_drawables(bitmap);
}
