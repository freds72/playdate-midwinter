
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
    float extents[2];

} GroundSlice;

// active slices + misc "globals"
typedef struct {
    // 
    float slice_y;
    float noise_y_offset;
    float y_offset;
    int plyr_z_index;
    int max_pz;

    GroundSlice* slices[GROUND_SIZE];
} Ground;

typedef struct {
    // contains all zoom levels
    LCDBitmap* scaled[_scaled_image_count];
} ScaledProp;

typedef struct {
    // -30 - 30 (inc.)
    ScaledProp rotated[61];
} RotatedScaledProp;

// 0: pine tree
static RotatedScaledProp _ground_props[8];

// raycasting angles
#define RAYCAST_PRECISION 128
static float _raycast_angles[RAYCAST_PRECISION];

// global buffer to store slices
static GroundSlice _slices_buffer[GROUND_SIZE];
// active ground
static Ground _ground;

static GroundParams active_params;

// large 32x32 pattern
typedef uint32_t DitherPattern[32];
static DitherPattern _dithers[16];

static uint32_t* _backgrounds[61];
static int _backgrounds_heights[61];

// returns a random number between 0-1
static float randf() {
  return (float)rand() / RAND_MAX;
}

// compute normal and assign material for faces
static void mesh_slice(int j) {
    GroundSlice* s0 = _ground.slices[j];
    GroundSlice* s1 = _ground.slices[j + 1];

    // base slope normal
    Point3d sn = { .x = 0, .y = GROUND_CELL_SIZE, .z = s0->y - s1->y };
    v_normz(&sn);

    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        const Point3d v0 = { .v = {i * GROUND_CELL_SIZE,s0->h[i] + s0->y,j * GROUND_CELL_SIZE} };
        // v1-v0
        const Point3d u1 = { .v = {GROUND_CELL_SIZE,s0->h[i + 1] + s0->y - v0.y,0} };
        // v2-v0
        const Point3d u2 = { .v = {GROUND_CELL_SIZE,s1->h[i + 1] + s1->y - v0.y,GROUND_CELL_SIZE} };
        // v3-v0
        const Point3d u3 = { .v = {0,s1->h[i] + s1->y - v0.y,GROUND_CELL_SIZE} };

        Point3d n0, n1;
        v_cross(&u3, &u2, &n0);
        v_cross(&u2, &u1, &n1);
        v_normz(&n0);
        v_normz(&n1);
        GroundFace* f0 = &s0->faces[2 * i];
        f0->n = n0;
        if (v_dot(&n0, &n1) > 0.999f) {
            f0->quad = 1;
        }
        else {
            f0->quad = 0;
            GroundFace* f1 = &s0->faces[2 * i + 1];
            f1->n = n1;
            f1->quad = 0;
        }
    }
}

static void make_slice(GroundSlice* slice, float y) {
    // smooth altitude changes
    _ground.slice_y = lerpf(_ground.slice_y, y, 0.2f);
    y = _ground.slice_y;
    // capture height
    slice->y = y;
    for (int i = 0; i < GROUND_SIZE; ++i) {
        slice->h[i] = (16.f*perlin2d((16.f * i) / GROUND_SIZE, _ground.noise_y_offset, 0.1f, 4) - 8.f) * active_params.slope;
        if (randf()>active_params.props_rate)
        {
            // todo: more types
            slice->props[i] = 1;
            slice->prop_t[i] = randf();
        }
        else {
            slice->props[i] = 0;
        }

    }
    // todo: control roughness from slope?
    _ground.noise_y_offset += 0.5f;
    // side walls
    slice->h[0] = slice->h[1] + 32.0f;
    slice->h[GROUND_SIZE - 1] = slice->h[GROUND_SIZE - 2] + 32.0f;

    slice->extents[0] = GROUND_CELL_SIZE;
    slice->extents[1] = (GROUND_SIZE - 2) * GROUND_CELL_SIZE;

    slice->is_checkpoint = 0;
}

void make_ground(GroundParams params) {
    pd->system->logToConsole("Ground params:\nslope:%f", params.slope);

    active_params = params;
    // reset global params
    _ground.slice_y = 0;
    _ground.y_offset = 0;
    _ground.noise_y_offset = 16.f * randf();
    _ground.plyr_z_index = GROUND_SIZE / 2 - 1;
    _ground.max_pz = INT_MIN;

    for (int i = 0; i < GROUND_SIZE; ++i) {
        // reset slices
        _ground.slices[i] = &_slices_buffer[i];
        make_slice(_ground.slices[i], -i * params.slope);
    }
    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        mesh_slice(i);
    }
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
        _ground.max_pz = p->z;
    }
}

void get_start_pos(Point3d* out) {
    out->x = GROUND_SIZE * GROUND_CELL_SIZE / 2;
    out->y = 0;
    out->z = _ground.plyr_z_index * GROUND_CELL_SIZE;
}

void get_face(Point3d pos, Point3d* nout, float* yout) {
    // z slice
    int i = (int)(pos.x / GROUND_CELL_SIZE), j = (int)(pos.z / GROUND_CELL_SIZE);
    
    const GroundSlice* s0 = _ground.slices[j];
    GroundFace* f0 = &s0->faces[2 * i];
    GroundFace* f1 = &s0->faces[2 * i + 1];
    GroundFace* f = f0;
    // select face
    if (!f0->quad && (pos.z - GROUND_CELL_SIZE * j < pos.x - GROUND_CELL_SIZE * i)) f = f1;

    // intersection point
    Point3d ptOnFace;
    make_v((Point3d) { .v = {i * GROUND_CELL_SIZE, s0->h[i] + s0->y - _ground.y_offset, j * GROUND_CELL_SIZE} }, pos, &ptOnFace);
         
    // 
    *yout = pos.y - v_dot(&ptOnFace, &f->n) / f->n.y;
    *nout = f->n;
    //         return f, p, atan2(slices[j + 2].x - p[1], 2 * dz)
}

static void load_noise(const int i, const int _) {
    const char* err;
    char* path = NULL;
    pd->system->formatString(&path, "images/generated/noise32x32-%d", i);
    LCDBitmap* bitmap = pd->graphics->loadBitmap(path, &err);
    if (!bitmap)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (w != 32 || h != 32)
        pd->system->logToConsole("Invalid pattern format: %dx%d, file: %s", w, h, path);
    for (int j = 0; j < 32; ++j) {
        int mask = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
        _dithers[i][j] = mask;
        data += 4;
    }
    // release source image
    pd->graphics->freeBitmap(bitmap);
}

static void load_background(const int i, const int _) {
    const char* err;
    char* path = NULL;
    // convert to "angle"
    pd->system->formatString(&path, "images/generated/sky_background_%i", i);
    LCDBitmap* bitmap = pd->graphics->loadBitmap(path, &err);
    if (!bitmap)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(bitmap, &w, &h, &r, &mask, &data);
    if (w != 400)
        pd->system->logToConsole("Invalid background format: %ix%i, expected 400x*, file: %s", w, h, path);
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
    _backgrounds[i + 30] = image;
    _backgrounds_heights[i + 30] = h;
    // release source image
    pd->graphics->freeBitmap(bitmap);
}

static void load_prop(const int angle, const int prop) {
    const char* err;
    for (int i = 0; i < _scaled_image_count; i++) {
        char* path = NULL;
        pd->system->formatString(&path, "images/generated/pine_snow_0_%i_%i", angle, i);

        LCDBitmap* bitmap = pd->graphics->loadBitmap(path, &err);
        if (!bitmap)
            pd->system->logToConsole("Failed to load: %s, %s", path, err);

        _ground_props->rotated[30 + angle].scaled[i] = bitmap;
    }
}

typedef void(* unit_of_work_callback)(const int, const int);
typedef struct {
    unit_of_work_callback callback;
    int param1;
    int param2;
} UnitOfWork;

static struct {
    UnitOfWork todo[512];
    int cursor;
    int n;
} _work;


void ground_init(PlaydateAPI* playdate) {

    // keep SDK handle   
    pd = playdate;

    _work.cursor = 0;
    _work.n = 0;

    // read dither table
    for (int i = 0; i < 16; ++i) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_noise,
            .param1 = i,
            .param2 = -1
        };
    }

    // read rotated backgrounds
    for (int i = -30; i < 30; i++) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_background,
            .param1 = i,
            .param2 = -1
        };
    }

    // read rotated & scaled sprites
    for (int angle = -30; angle < 31; ++angle) {
        _work.todo[_work.n++] = (UnitOfWork){
            .callback = load_prop,
            .param1 = angle,
            .param2 = 1
        };
    }

    // raycasting angles
    for(int i=0;i<RAYCAST_PRECISION;++i) {
        _raycast_angles[i] = atan2f(31.5f,(float)(i-63.5f)) - PI/2.f;
    }
}

int ground_load_assets_async() {
    // done
    if (_work.cursor >= _work.n) return 0;

    UnitOfWork* activeUnit = &_work.todo[_work.cursor++];
    (*activeUnit->callback)(activeUnit->param1, activeUnit->param2);
    return 1;
}

// --------------- 3d rendering -----------------------------
#define Z_NEAR 1
#define OUTCODE_FAR 0
#define OUTCODE_NEAR 2
#define OUTCODE_RIGHT 4
#define OUTCODE_LEFT 8

typedef struct {
    float x;
    float y;
    float z;
    int outcode;
} CameraPoint3d;

static struct {
    // visible tiles encoded as 1 bit per cell
    uint32_t visited[GROUND_SIZE];
    // front to back visible tiles
    uint32_t ordered[GROUND_SIZE * GROUND_SIZE];
    // number of visible tiles
    int n;
} _visible_tiles;

// clip polygon against near-z
static int z_poly_clip(const float znear, Point3d* in, int n, Point3d* out) {
    Point3d v0 = in[n - 1];
    float d0 = v0.z - znear;
    int nout = 0;
    for (int i = 0; i < n; i++) {
        Point3d v1 = in[i];
        int side = d0 > 0;
        if (side) out[nout++] = (Point3d){ .v = { v0.x, v0.y, v0.z } };
        float d1 = v1.z - znear;
        if ((d1 > 0) != side) {
            // clip!
            float t = d0 / (d0 - d1);
            out[nout++] = (Point3d){ .v = {
                lerpf(v0.x,v1.x,t),
                lerpf(v0.y,v1.y,t),
                znear
            } };
        }
        v0 = v1;
        d0 = d1;
    }
    return nout;
}

// draw a face
static void draw_face(const float* m, const Point3d* p, int* indices, int n, uint32_t* bitmap) {
    Point3d tmp[4];
    // transform
    int outcode = 0xfffffff, is_clipped = 0;
    float min_key = FLT_MAX;
    for (int i = 0; i < n; ++i) {
        Point3d* res = &tmp[i];
        // project using active matrix
        m_x_v(m, p[indices[i]], res);
        int code = res->z > Z_NEAR ? OUTCODE_FAR : OUTCODE_NEAR;
        if (res->x > res->z) code |= OUTCODE_RIGHT;
        if (-res->x > res->z) code |= OUTCODE_LEFT;
        outcode &= code;
        is_clipped += code & 2;
        if (res->z < min_key) min_key = res->z;
    }

    // visible?
    if (outcode == 0) {
        // clipped points in camera space
        Point3d pts[5];
        if (is_clipped > 0) {
            n = z_poly_clip(Z_NEAR, tmp, n, pts);
        }
        else {
            for (int i = 0; i < n; ++i) {
                pts[i] = tmp[i];
            }
        }
        for (int i = 0; i < n; ++i) {
            // project 
            float w = 199.5f / pts[i].z;
            pts[i].x = 199.5f + w * pts[i].x;
            pts[i].y = 119.5f - w * pts[i].y;
        }
        // todo: dither based on material + distance
        int dither_key = (int)(min_key / 4.f);
        if (dither_key > 15) dither_key = 15;
        if (dither_key < 0) dither_key = 0;
        polyfill(pts, n, _dithers[dither_key], bitmap);
        /*
        float x0 = pts[face->n - 1].x, y0 = pts[face->n - 1].y;
        for (int i = 0; i < face->n; ++i) {
            float x1 = pts[i].x, y1 = pts[i].y;
            pd->graphics->drawLine(x0,y0,x1,y1, 1, kColorWhite);
            x0 = x1, y0 = y1;
        }
        */
    }
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
    if (angle < -30) angle = -30;
    if (angle > 30) angle = 30;
    uint32_t* src = _backgrounds[angle + 30];

    int h = _backgrounds_heights[angle + 30];

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
    memcpy(bitmap + h0 * LCD_ROWSIZE / sizeof(uint32_t), src, (h1-h0) * LCD_ROWSIZE);
    memset(bitmap + h1 * LCD_ROWSIZE / sizeof(uint32_t), 0x00, (LCD_ROWS - h1) * LCD_ROWSIZE);

    return angle;
}

static void collect_tiles(const Point3d pos, float base_angle) {
    float x = pos.x / GROUND_CELL_SIZE, y = pos.z / GROUND_CELL_SIZE;
    int x0 = (int)x, y0 = (int)y;

    // current tile
    if (!(_visible_tiles.visited[y0] & (1 << x0))) {
        _visible_tiles.visited[y0] |= 1 << x0;
        _visible_tiles.ordered[_visible_tiles.n++] = x0 + y0 * GROUND_SIZE;
    }
    for (int i = 0; i < RAYCAST_PRECISION; ++i) {
        float angle = base_angle + _raycast_angles[i];
        float v = cosf(angle), u = sinf(angle);

        int mapx = x0, mapy = y0;
        float ddx = 1.f / u, ddy = 1.f / v;
        float mapdx = 1, mapdy = 1;
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

            if(!(_visible_tiles.visited[mapy] & (1 << mapx))) {
                _visible_tiles.visited[mapy] |= 1 << mapx;
                _visible_tiles.ordered[_visible_tiles.n++] = mapx + mapy * GROUND_SIZE;
            }
        }
    }
}

// render ground
void render_ground(Point3d cam_pos, float cam_angle, float* m, uint32_t* bitmap) {
    int angle = render_sky(m, bitmap);

    // collect visible tiles
    // reset tiles
    _visible_tiles.n = 0;
    memset(_visible_tiles.visited, 0, sizeof(uint32_t) * GROUND_SIZE);

    collect_tiles(cam_pos, cam_angle);

    // transform visible tiles
    const float y_offset = _ground.y_offset;
    for (int k = _visible_tiles.n - 1; k >= 0; k--) {
        const int tile_id = _visible_tiles.ordered[k];
        const int i = tile_id % GROUND_SIZE, j = tile_id / GROUND_SIZE;
        if (j < GROUND_SIZE - 1) {
            const GroundSlice* s0 = _ground.slices[j];
            const GroundSlice* s1 = _ground.slices[j + 1];
            const Point3d verts[4] = {
            {.v = {i * GROUND_CELL_SIZE,s0->h[i] + s0->y - y_offset,j * GROUND_CELL_SIZE}},
            {.v = {(i + 1) * GROUND_CELL_SIZE,s0->h[i + 1] + s0->y - y_offset,j * GROUND_CELL_SIZE}},
            {.v = {(i + 1) * GROUND_CELL_SIZE,s1->h[i + 1] + s1->y - y_offset,(j + 1) * GROUND_CELL_SIZE}},
            {.v = {i * GROUND_CELL_SIZE,s1->h[i] + s1->y - y_offset,(j + 1) * GROUND_CELL_SIZE}} };
            // transform
            const GroundFace* f0 = &s0->faces[2 * i];
            // camera to face point
            const Point3d cv = { .x = verts[0].x - cam_pos.x,.y = verts[0].y - cam_pos.y,.z = verts[0].z - cam_pos.z };
            if (f0->quad) {
                if (v_dot(&f0->n, &cv) < 0.f)
                {
                    draw_face(m, verts, (int[]) { 0, 1, 2, 3 }, 4, bitmap);
                }
            }
            else {
                const GroundFace* f1 = &s0->faces[2 * i + 1];
                if (v_dot(&f0->n, &cv) < 0.f)
                {
                    draw_face(m, verts, (int[]) { 0, 2, 3 }, 3, bitmap);
                }
                if (v_dot(&f1->n, &cv) < 0.f)
                {
                    draw_face(m, verts, (int[]) { 0, 1, 2 }, 3, bitmap);
                }
            }
            // draw prop (if any)
            if (s0->props[i] != 0) {
                Point3d pos, res;
                v_lerp(&verts[0], &verts[2], s0->prop_t[i], &pos);
                m_x_v(m, pos, &res);
                if (res.z > Z_NEAR && res.z < GROUND_CELL_SIZE * 16) {
                    float w = 199.5f / res.z;
                    float x = 199.5f + w * res.x;
                    float y = 119.5f - w * res.y;
                    int bw, bh;
                    int stride;
                    LCDBitmap* bitmap = _ground_props->rotated[angle + 30].scaled[_scaled_by_z[(int)(16 * (res.z / GROUND_CELL_SIZE - 1.f))]];
                    if (bitmap) {
                        uint8_t* dummy;
                        pd->graphics->getBitmapData(bitmap, &bw, &bh, &stride, &dummy, &dummy);
                        pd->graphics->drawBitmap(bitmap, x - bw / 2, y - bh, 0);
                    }
                }
            }
        }
    }
}

