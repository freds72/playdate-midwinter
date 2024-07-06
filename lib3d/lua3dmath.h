#ifndef lua3dmath_h
#define lua3dmath_h

#include "3dmath.h"
#include "luaglue.h"
#include "userdata.h"

// interface with C/lua stack
Point3d* getArgVec3(int n);
Mat4* getArgMat4(int n);
void pushArgVec3(Point3d* p);
void pushArgMat4(Mat4* p);

void lua3dmath_init(PlaydateAPI* playdate);

#endif