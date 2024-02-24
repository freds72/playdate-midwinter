#ifndef lib3d_gfx_h
#define lib3d_gfx_h

#include "3dmath.h"
#include "3dmathi.h"

void gfx_init(PlaydateAPI* playdate);
void polyfill(const Point3d* verts, const int n, uint32_t* dither, uint32_t* bitmap);
void texfill(const Point3d* verts, const int n, uint32_t* dither_ramp, uint32_t* bitmap);
void trifill(const Point2di* v0, const Point2di* v1, const Point2di* v2, uint8_t* bitmap);
void sspr(int x, int y, int w, uint8_t* src, int sw, uint8_t* bitmap);
void upscale_image(int x, int y, int w, int h, uint8_t* src, int sw, int sh, uint8_t* bitmap);

#endif
