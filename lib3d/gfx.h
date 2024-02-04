#ifndef lib3d_gfx_h
#define lib3d_gfx_h

#include "3dmath.h"
#include "3dmathi.h"

void polyfill(const Point3d* verts, const int n, uint32_t* dither, uint32_t* bitmap);
void trifill(const Point2di* v0, const Point2di* v1, const Point2di* v2, uint8_t* bitmap);

#endif
