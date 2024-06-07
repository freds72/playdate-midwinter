#ifndef _particles_h
#define _particles_h

#include <pd_api.h>
#include "3dmath.h"
#include "drawables.h"

// ***********************
// particle system

typedef struct {
    int age;
    int ttl;
    float angle;
    float angularv;
    float radius;
    float radius_decay;
    Point3d pos;
} Particle;

#define EMITTER_SNOW_TRAIL 0

void particles_init(PlaydateAPI* playdate);
Particle* make_particle(int id, Point3d pos);
void update_particles(Point3d offset);
void push_particles(Drawables* drawables, Point3d cam_pos, float* m);

#endif