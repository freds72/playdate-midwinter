#ifndef _drawables_h
#define _drawables_h

#include <pd_api.h>
#include "3dmath.h"

#define Z_NEAR 0.5f
#define Z_FAR 64.f

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

typedef struct {
    int material;
    float radius;
    float angle;
    Point3d pos;
} DrawableParticle;

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
        DrawableParticle particle;
    };
} Drawable;

typedef struct {
    int n;
    // arbitrary limit
    Drawable all[4096];
} Drawables;

void drawables_init(PlaydateAPI* playdate);

#endif
