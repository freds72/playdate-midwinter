#include <math.h>
#include "3dmath.h"

void make_v(const Point3d a, const Point3d b, Point3d* out) {
    out->x = b.x - a.x;
    out->y = b.y - a.y;
    out->z = b.z - a.z;
}

void v_normz(Point3d* a) {
    const float d = sqrtf(a->x*a->x + a->y*a->y + a->z*a->z);
    if (fabs(d) < 0.0001f) return;
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

void m_x_v(const float* m, const Point3d v,Point3d *out) {
	float x=v.x,y=v.y,z=v.z;
    out->x = m[0]*x+m[4]*y+m[8]*z+m[12];
    out->y = m[1]*x+m[5]*y+m[9]*z+m[13];
    out->z = m[2]*x+m[6]*y+m[10]*z+m[14];	
}

float v_dot(const Point3d* a, const Point3d* b) {
  return 
    a->x * b->x +
    a->y * b->y +
    a->z * b->z;
}