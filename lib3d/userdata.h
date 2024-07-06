#ifndef _userdata_h
#define _userdata_h

#include <pd_api.h>
#include "3dmath.h"

void userdata_init(PlaydateAPI* playdate);
void userdata_stats(int* vlen, int* vmax, int* mlen, int* mmax);

// pop a vec3 from the buffer
Point3d* pop_vec3();
// restores a vec3 pointer
void push_vec3(Point3d* p);

// pop a mat4x4 from the buffer
Mat4* pop_mat4();
// restores a mat4x4 pointer
void push_mat4(Mat4* p);

#endif