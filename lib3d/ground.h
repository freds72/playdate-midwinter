#ifndef _ground_h
#define _ground_h

#include "ground_limits.h"
#include "3dmath.h"

typedef struct {
  float slope;
  float props_rate;
  float twist;
  int tight_mode;
  // number of tracks
  int num_tracks;
  int track_type;
  // min/max time between sections
  int min_cooldown;
  int max_cooldown;
  // random seed
  int r_seed;
} GroundParams;

typedef struct {
	int type;
	Point3d pos;
} PropInfo;

// create a new ground
void make_ground(GroundParams params);

// start position (to be called after make_ground)
void get_start_pos(Point3d* out);

// return face details at given position
// returns 0 if out of bounds
int get_face(const Point3d pos, Point3d* n, float* y);
 
// get track extent & direction
void get_track_info(const Point3d pos, float* xmin, float* xmax, float*z, int* checkpoint, float* angle);

// clear checkpoint flag at pos
void clear_checkpoint(const Point3d pos);

// update ground, create new slice as necessary and adjust position
// offset contains the ground "position" offset when slices are created
void update_ground(const Point3d pos, int* slice_id, char** pattern, Point3d* offset);

// check collision
void collide(Point3d pos, float radius, int* hit_type);

// render ground
void render_ground(const Point3d pos, const float tau_angle, const Mat4 m, uint32_t blink, uint8_t* bitmap);

// register a new "free" prop to be rendered using the given transformation matrix
void add_render_prop(const int id, const Mat4 m);

// render "free" props
void render_props(const Point3d pos, const Mat4 m, uint8_t* bitmap);

// load stuff (to be called until returns 0)
int ground_load_assets_async();

// init module
void ground_init(PlaydateAPI* playdate);

#endif 