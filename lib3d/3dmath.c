#include <math.h>
#include <stdlib.h>
#include "3dmath.h"

// misc. helpers

// returns a random number between 0-1
float randf() {
    return (float)rand() / RAND_MAX;
}

int randi(int max) {
    return (int)(max * randf());
}

/* Arrange the N elements of ARRAY in random order.
   Only effective if N is much smaller than RAND_MAX;
   if this may not be the case, use a better random
   number generator. */
void shuffle(int* array, size_t n)
{
    if (n > 1) {
        size_t i;
        for (i = 0; i < n - 1; i++) {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

// vector helpers
void make_v(const Point3d a, const Point3d b, Point3d* out) {
    out->x = b.x - a.x;
    out->y = b.y - a.y;
    out->z = b.z - a.z;
}

void v_normz(Point3d* a) {
    const float d = sqrtf(a->x*a->x + a->y*a->y + a->z*a->z);
    if (fabsf(d) < 0.0001f) return;
    a->x /= d;
    a->y /= d;
    a->z /= d;
}

void v_cross(const Point3d* a, const Point3d* b, Point3d* out) {
    const float ax = a->x, ay = a->y, az = a->z;
    const float bx = b->x, by = b->y, bz = b->z;
    out->x = ay * bz - az * by;
    out->y = az * bx - ax * bz;
    out->z = ax * by - ay * bx;
}

void m_x_v(const float* m, const Point3d v,float *out) {
	float x=v.x,y=v.y,z=v.z;
    out[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
    out[1] = m[1]*x+m[5]*y+m[9]*z+m[13];
    out[2] = m[2]*x+m[6]*y+m[10]*z+m[14];
}

float v_dot(const Point3d* a, const Point3d* b) {
  return 
    a->x * b->x +
    a->y * b->y +
    a->z * b->z;
}

void v_lerp(const Point3d* a, const Point3d* b, const float t, Point3d* out) {
    out->x = lerpf(a->x, b->x, t);
    out->y = lerpf(a->y, b->y, t);
    out->z = lerpf(a->z, b->z, t);
}
