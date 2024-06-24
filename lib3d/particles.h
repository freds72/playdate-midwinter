#ifndef _particles_h
#define _particles_h

#include <pd_api.h>
#include "3dmath.h"

// ***********************
// particle system

#define EMITTER_SNOW_TRAIL 0
#define EMITTER_SMOKE 1

void particles_init(PlaydateAPI* playdate);
void clear_particles();
void spawn_particle(int id, Point3d pos);
void update_particles(Point3d offset);
void push_particles(Point3d cam_pos, float* m);

#endif