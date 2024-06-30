#ifndef _userdata_h
#define _userdata_h

#include <pd_api.h>
#include "3dmath.h"

void userdata_init(PlaydateAPI* playdate);
// pop a vec3 from the buffer
float* pop_vec3();
// restores a vec3 pointer
void push_vec3(float* p);

// pop a mat4x4 from the buffer
float* pop_mat4();
// restores a mat4x4 pointer
void push_mat4(float* p);

#endif