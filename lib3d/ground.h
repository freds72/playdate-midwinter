#ifndef _ground_h
#define _ground_h

#include "3dmath.h"

typedef struct {
  Point3d n;  
  // 0: triangle
  // 1: quad
  int quad;
  int material;
} GroundFace;

typedef struct {
  float slope;
  float bonus_t;
  float total_t;
  float props_rate;
  int props[16];
  int num_tracks;
} GroundParams;

typedef struct {
	int is_checkpoint;
	float xmin;
	float xmax;

	void* impl;
} TrackInfo;

// create a new ground
void make_ground(GroundParams params);

// start position (to be called after make_ground)
void get_start_pos(Point3d* out);

// return face details at given position
void get_face(Point3d pos, Point3d* n, float* y,float* angle);

// get track extent
void get_track_info(Point3d pos, float* xmin, float* xmax, float*z, int* checkpoint);
// clear checkpoint flag at pos
void clear_checkpoint(Point3d pos);

// update ground, create new slice as necessary and adjust position
void update_ground(Point3d* pos);

// death animation
void update_snowball(Point3d pos, int rotation);

// check collision
void collide(Point3d pos, float radius, int* hit_type);

// render ground
void render_ground(Point3d pos, float angle, float*m, uint32_t* bitmap);

// load stuff (to be called until returns 0)
int ground_load_assets_async();

// init module
void ground_init(PlaydateAPI* playdate);

#endif 