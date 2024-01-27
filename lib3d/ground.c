
#include <stdlib.h>
#include <pd_api.h>
#include <limits.h>
#include <float.h>
#include "ground.h"
#include "perlin.h"
#include "gfx.h"
#include "3dmath.h"

#define GROUND_SIZE 32
#define GROUND_CELL_SIZE 4

typedef struct {
  float y;
  // faces (normal + material only)
  GroundFace faces[GROUND_SIZE*2];
  // height
  float h[GROUND_SIZE];
  // actor type (0 if none)
  int props[GROUND_SIZE];
  Point3d props_pos[GROUND_SIZE];

  // 
  int is_checkpoint;
  // main track extents
  float extents[2];

} GroundSlice;

// active slices
typedef struct {
  GroundSlice* slices[GROUND_SIZE];
} Ground;

// global buffer to store slices
static GroundSlice _slices_buffer[GROUND_SIZE];
// active ground
static Ground _ground;

static float slice_y = 0;
static float noise_y_offset = 0;
static float _y_offset = 0;
static int plyr_z_index = 0;
static float max_pz = 0;

static GroundParams active_params;

// large 32x32 pattern
typedef uint32_t DitherPattern[32];
static DitherPattern _dithers[16];

// returns a random number between 0-1
static float randf() {
  return rand() / RAND_MAX;
}

// compute normal and assign material for faces
static void mesh_slice(int j) {
    GroundSlice* s0 = _ground.slices[j];
    GroundSlice* s1 = _ground.slices[j + 1];

    // base slope normal
    Point3d sn = { .x = 0, .y = GROUND_CELL_SIZE, .z = s0->y - s1->y };
    v_normz(&sn);

    for (int i = 0; i < GROUND_SIZE - 1; ++i) {
        Point3d v0 = { .v = {i * GROUND_CELL_SIZE,s0->h[i] + s0->y,j * GROUND_CELL_SIZE} };
        // v1-v0
        Point3d u1 = { .v = {GROUND_CELL_SIZE,s0->h[i + 1] - s0->h[i],0} };
        // v2-v0
        Point3d u2 = { .v = {GROUND_CELL_SIZE,s1->h[i + 1] + s1->y - v0.y,GROUND_CELL_SIZE} };
        // v3-v0
        Point3d u3 = { .v = {0,s1->h[i] + s1->y - v0.y,GROUND_CELL_SIZE} };

        Point3d n0, n1;
        v_cross(&u3, &u2, &n0);
        v_cross(&u2, &u1, &n1);
        v_normz(&n0);
        v_normz(&n1);
        if (v_dot(&n0, &n1) > 0.995f) {
            GroundFace* f0 = &s0->faces[2 * i];
            f0->n = n0;
            f0->quad = 1;
        }
        else {
            GroundFace* f0 = &s0->faces[2 * i];
            f0->n = n0;
            f0->quad = 0;
            GroundFace* f1 = &s0->faces[2 * i + 1];
            f1->n = n1;
            f1->quad = 0;
        }
    }
}

static void make_slice(GroundSlice* slice, float y) {
    // smooth altitude changes
    slice_y = lerpf(slice_y, y, 0.2f);
    y = slice_y;
    // capture height
    slice->y = y;
    for (int i = 0; i < GROUND_SIZE; ++i) {
        slice->h[i] = 32.0f * perlin2d((16.f * i) / GROUND_SIZE, noise_y_offset, 0.1f, 4) * active_params.slope;
        slice->props[i] = 0;
    }
    noise_y_offset += 0.5f;
    // side walls
    slice->h[0] = 15.f + 5.f * randf();
    slice->h[GROUND_SIZE - 1] = 15.f + 5.f * randf();

    slice->extents[0] = GROUND_CELL_SIZE;
    slice->extents[1] = (GROUND_SIZE - 2) * GROUND_CELL_SIZE;

    slice->is_checkpoint = 0;
}

void make_ground(GroundParams params) {
    active_params = params;
    // reset global params
    slice_y = 0;
    noise_y_offset = 16.f * randf();
    plyr_z_index = GROUND_SIZE / 2 - 1;
    max_pz = INT_MIN;

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
    // prevent out of bounds
    if (p->z < 8 * GROUND_CELL_SIZE) p->z = 8 * GROUND_CELL_SIZE;
    float pz = p->z / GROUND_CELL_SIZE;
    if (pz > plyr_z_index) {
        // shift back
        p->z -= GROUND_CELL_SIZE;
        max_pz -= GROUND_CELL_SIZE;
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
    if (p->z > max_pz) {
        _y_offset = lerpf(_ground.slices[0]->y, _ground.slices[1]->y, pz - (int)pz);
        max_pz = p->z;
    }
}

void get_start_pos(Point3d* out) {
    out->x = GROUND_SIZE * GROUND_CELL_SIZE / 2;
    out->y = 0;
    out->z = plyr_z_index * GROUND_CELL_SIZE;
}

void get_face(Point3d pos, Point3d* nout, float* yout) {
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
    make_v((Point3d) { .v = {i * GROUND_CELL_SIZE, s0->h[i] + s0->y - _y_offset, j * GROUND_CELL_SIZE} }, pos, & ptOnFace);
    float t = -v_dot(&ptOnFace, &f->n) / f->n.y;
         
    // 
    *yout = pos.y + t;
    *nout = f->n;
    //         return f, p, atan2(slices[j + 2].x - p[1], 2 * dz)
}

void ground_load_assets(PlaydateAPI* playdate) {
  PlaydateAPI* pd = playdate;

	const char* err;  
  // read dither table
  for (int i = 0; i < 16; ++i) {
    char* path = NULL;
    pd->system->formatString(&path, "images/generated/noise32x32-%d", i);
    LCDBitmap* dither = pd->graphics->loadBitmap(path, &err);
    if (!dither)
        pd->system->logToConsole("Failed to load: %s, %s", path, err);
    int w = 0, h = 0, r = 0;
    uint8_t* mask = NULL;
    uint8_t* data = NULL;
    pd->graphics->getBitmapData(dither, &w, &h, &r, &mask, &data);
    if (w!=32 || h!=32) 
        pd->system->logToConsole("Invalid pattern format: %dx%d, file: %s", w, h, path);
    for (int j = 0; j < 32; ++j) {
        int mask = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
        _dithers[i][j] = mask;
        data += 4;
    }
  }  
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

typedef struct {
    // clipped points in camera space
    Point3d pts[5];
    // number of points
    int n;
    int material;
    // sort key
    float key;
} Face;

// clip incomping polygon
static int z_poly_clip(float znear, Point3d* in, int n, Point3d* out) {	
  Point3d v0 = in[n-1];
	float d0 = v0.z - znear;
  int nout = 0;
  for(int i=0;i<n;i++) {
    Point3d v1 = in[i];
    int side = d0>0;
    if (side) out[nout++] = (Point3d){v0.x, v0.y, v0.z};
    float d1 = v1.z - znear;
    if((d1>0)!=side) {
    // clip!
      float t = d0/(d0-d1);
      out[nout++] = (Point3d){
          lerpf(v0.x,v1.x,t),
          lerpf(v0.y,v1.y,t),
          znear
      };
    }
    v0 = v1;
    d0 = d1;        
  }
  return nout;
}

static struct {
  int tiles[GROUND_SIZE * GROUND_SIZE];
  int n;
} _visible_tiles;

static struct {
  Face faces[GROUND_SIZE * GROUND_SIZE * 2];
  int n;
} _drawables;

static Face* _sortables[GROUND_SIZE * GROUND_SIZE * 2];

static int cmp_face(const void * a, const void * b) {
  float x = ((Face*)a)->key;
  float y = ((Face*)b)->key;
  return x < y ? -1 : x == y ? 0 : 1;
}

static void add_drawable_face(float* m,Point3d* p,int* indices, int n) {
  Point3d tmp[4];
  // transform
  int outcode = 0xfffffff, is_clipped = 0;
  float min_key = FLT_MAX;
  for(int i=0;i<n;++i) {          
    Point3d* res = &tmp[i];
    // project using active matrix
    m_x_v(m,p[indices[i]], res);
    int code = res->z>Z_NEAR?OUTCODE_FAR:OUTCODE_NEAR;
    if(res->x>res->z) code|=OUTCODE_RIGHT;
    if(-res->x>res->z) code|=OUTCODE_LEFT;
    outcode &= code;
    is_clipped += code&2;
    if(res->z<min_key) min_key = res->z;
  }

  // visible?
  if (outcode==0) {
    Face* face = &_drawables.faces[_drawables.n++];
    face->key = min_key;
    if (is_clipped>0) {
      face->n = z_poly_clip(Z_NEAR, tmp, n, face->pts);
    } else {
      face->n = n;
      for(int i=0;i<n;++i) {
        face->pts[i] = tmp[i];
      }
    }   
  }  
}

void render_ground(Point3d cam_pos, float* m, uint32_t* bitmap) {
    // todo: collect visible tiles
    _visible_tiles.n = (GROUND_SIZE-1) * GROUND_SIZE;
    for (int i = 0; i < _visible_tiles.n; ++i) {
        _visible_tiles.tiles[i] = i;
    }

    // transform visible tiles
    _drawables.n = 0;
    for (int k = 0; k < _visible_tiles.n; ++k) {
        int tileid = _visible_tiles.tiles[k];
        int i = tileid % GROUND_SIZE, j = tileid / GROUND_SIZE;
        GroundSlice* s0 = _ground.slices[j];
        GroundSlice* s1 = _ground.slices[j + 1];
        Point3d verts[4] = {
          {.v = {i * GROUND_CELL_SIZE,s0->h[i] + s0->y - _y_offset,j * GROUND_CELL_SIZE}},
          {.v = {(i + 1) * GROUND_CELL_SIZE,s0->h[i + 1] + s0->y - _y_offset,j * GROUND_CELL_SIZE}},
          {.v = {(i + 1) * GROUND_CELL_SIZE,s1->h[i + 1] + s1->y - _y_offset,(j + 1) * GROUND_CELL_SIZE}},
          {.v = {i * GROUND_CELL_SIZE,s1->h[i] + s1->y - _y_offset,(j + 1) * GROUND_CELL_SIZE}} };
        // transform
        GroundFace* f0 = &s0->faces[2 * i];
        // camera to face point
        Point3d cv = { .x = verts[0].x - cam_pos.x,.y = verts[0].y - cam_pos.y,.z = verts[0].z - cam_pos.z };
        if (f0->quad) {
            if (v_dot(&f0->n, &cv) < 0.f) 
            {
                add_drawable_face(m, verts, (int[]) { 0, 1, 2, 3 }, 4);
            }
        }
        else {
            GroundFace* f1 = &s0->faces[2 * i + 1];
            if (v_dot(&f0->n, &cv) < 0.f) 
            {
                add_drawable_face(m, verts, (int[]) { 0, 1, 2 }, 3);
            }
            if (v_dot(&f1->n, &cv) < 0.f) 
            {
                add_drawable_face(m, verts, (int[]) { 0, 2, 3 }, 3);
            }
        }
    }

    // sort
    if (_drawables.n > 0) {
        for (int i = 0; i < _drawables.n; ++i) {
            _sortables[i] = &_drawables.faces[i];
        }
        qsort(_sortables, _drawables.n, sizeof(Face*), &cmp_face);

        // rendering
        for (int k = _drawables.n - 1; k >= 0;--k) {
            Face* face = _sortables[k];
            Point3d* pts = face->pts;
            for (int i = 0; i < face->n; ++i) {
                // project 
                float w = 199.5f / pts[i].z;
                pts[i].x = 199.5f + w * pts[i].x;
                pts[i].y = 119.5f - w * pts[i].y;
            }
            // todo: dither based on material + distance
            int dither_key = (int)(face->key / 4.f);
            if (dither_key > 15) dither_key = 15;
            if (dither_key < 0) dither_key = 0;
            polyfill(face->pts, face->n, _dithers[dither_key], bitmap);
        }
    }
}

