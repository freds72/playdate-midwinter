#ifndef lib3d_gfx_h
#define lib3d_gfx_h

#include "3dmath.h"

void gfx_init(PlaydateAPI* playdate);
void polyfill(const Point3du* verts, const int n, uint32_t* dither, uint32_t* bitmap);
void texfill(const Point3du* verts, const int n, uint8_t* dither_ramp, uint8_t* bitmap);
void alphafill(const Point3du* verts, const int n, uint32_t color, uint32_t* alpha, uint32_t* bitmap);

#endif
