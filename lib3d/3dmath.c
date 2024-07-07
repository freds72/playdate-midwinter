#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "3dmath.h"

// misc. helpers

// returns a random number between 0-1
float randf() {
    return (float)rand() / RAND_MAX;
}

int randi(int max) {
    return rand()%max;
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
    out->v[0] = b.v[0] - a.v[0];
    out->v[1] = b.v[1] - a.v[1];
    out->v[2] = b.v[2] - a.v[2];
}

float v_normz(Point3d* v) {
    const float x = v->x * v->x + v->y * v->y + v->z * v->z;

    //fast invsqrt approx
    Flint a = { .f = x };
    a.i = 0x5F3759DF - (a.i >> 1);		//VRSQRTE
    float c = x * a.f;
    float b = (3.0f - c * a.f) * 0.5f;		//VRSQRTS
    a.f *= b;
    c = x * a.f;
    b = (3.0f - c * a.f) * 0.5f;
    a.f *= b;

    v->v[0] *= a.f;
    v->v[1] *= a.f;
    v->v[2] *= a.f;

    return a.f;
}

void v_cross(const Point3d a, const Point3d b, Point3d* out) {
    const float ax = a.v[0], ay = a.v[1], az = a.v[2];
    const float bx = b.v[0], by = b.v[1], bz = b.v[2];
    out->v[0] = ay * bz - az * by;
    out->v[1] = az * bx - ax * bz;
    out->v[2] = ax * by - ay * bx;
}

void m_x_m(const Mat4 a, const Mat4 b, Mat4 out) {
    const float a11 = a[0], a12 = a[4], a13 = a[8], a21 = a[1],  a22 = a[5], a23 = a[9], a31 = a[2], a32 = a[6],  a33 = a[10];
    const float b11 = b[0], b12 = b[4], b13 = b[8], b14 = b[12], b21 = b[1], b22 = b[5], b23 = b[9], b24 = b[13], b31 = b[2], b32 = b[6], b33 = b[10], b34 = b[14];

    out[0] = a11 * b11 + a12 * b21 + a13 * b31;
    out[1] = a21 * b11 + a22 * b21 + a23 * b31;
    out[2] = a31 * b11 + a32 * b21 + a33 * b31;
    out[3] = 0.f;

    out[4] = a11 * b12 + a12 * b22 + a13 * b32;
    out[5] = a21 * b12 + a22 * b22 + a23 * b32;
    out[6] = a31 * b12 + a32 * b22 + a33 * b32;
    out[7] = 0.f;

    out[8] = a11 * b13 + a12 * b23 + a13 * b33;
    out[9] = a21 * b13 + a22 * b23 + a23 * b33;
    out[10] = a31 * b13 + a32 * b23 + a33 * b33;
    out[11] = 0.f;

    out[12] = a11 * b14 + a12 * b24 + a13 * b34 + a[12];
    out[13] = a21 * b14 + a22 * b24 + a23 * b34 + a[13];
    out[14] = a31 * b14 + a32 * b24 + a33 * b34 + a[14];
    out[15] = 1.f;
}

void m_x_translate(const Mat4 a, const Point3d pos, Mat4 out) {
    memcpy(out, a, sizeof(Mat4));
    
    const float x = pos.x, y = pos.y, z = pos.z;
    out[12] = a[0] * x + a[4] * y + a[8] * z + a[12];
    out[13] = a[1] * x + a[5] * y + a[9] * z + a[13];
    out[14] = a[2] * x + a[6] * y + a[10] * z + a[14];
}

void m_inv_x_v(const Mat4 m, const Point3d v, Point3d* out) {
    const float x = v.x - m[12], y = v.y - m[13], z = v.z - m[14];
    out->v[0] = m[0] * x + m[1] * y + m[2] * z;
    out->v[1] = m[4] * x + m[5] * y + m[6] * z;
    out->v[2] = m[8] * x + m[9] * y + m[10] * z;
}

void v_lerp(const Point3d a, const Point3d b, const float t, Point3d* out) {
    out->v[0] = lerpf(a.v[0], b.v[0], t);
    out->v[1] = lerpf(a.v[1], b.v[1], t);
    out->v[2] = lerpf(a.v[2], b.v[2], t);
}

void m_x_y_rot(const Mat4 a, const float angle, Mat4 out) {
    const float c = cosf(angle), s = sinf(angle);
    const float a11 = a[0], a13 = a[8], a21 = a[1], a23 = a[9], a31 = a[2], a33 = a[10];

    out[0] = a11 * c - a13 * s;
    out[1] = a21 * c - a23 * s;
    out[2] = a31 * c - a33 * s;
    out[3] = 0.f;

    out[8]  = a11 * s + a13 * c;
    out[9]  = a21 * s + a23 * c;
    out[10] = a31 * s + a33 * c;
    out[11] = 0.f;

    out[4] = a[4];
    out[5] = a[5];
    out[6] = a[6];
    out[7] = 0.f;


    out[12] = a[12];
    out[13] = a[13];
    out[14] = a[14];
    out[15] = 1.f;
}
