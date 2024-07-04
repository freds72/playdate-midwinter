#ifndef _lib3d_math_h
#define _lib3d_math_h

#include <stdint.h>

#define PI 3.1415927410125732421875f
#define MAT4x4 16
#define VEC3 3

#ifndef max
    #define max(a,b) (((a) > (b)) ? (a) : (b))
    #define min(a,b) (((a) < (b)) ? (a) : (b))
#endif // !max

// misc. math typedefs
typedef struct Point3d {
  union {
    // named access
    struct {
      float x;
      float y;
      float z;
    };
    // values array
    float v[VEC3];
  };
} Point3d;

typedef struct {
    union {
        struct {
            float x;
            float y;
        };
        float v[2];
    };
} Point2d;


// 3d point with u coordinate
typedef struct {
    struct Point3d;
    float u;
    float light;
} Point3du;

// range helpers
typedef struct {
    int min;
    int max;
} IntRange;

typedef struct {
    float min;
    float max;
} FloatRange;

// aliases a float to a 32bits memory address
typedef struct {
    union {
        float f;
        uint32_t i;
    };
} Flint;

// matrix struct
typedef float Mat4[MAT4x4];

// convert a tau angle [0;1] into a radian angle
inline float detauify(const float tau) {
    return tau * 2 * PI;
}

// convert a rad angle [0;2*PI] into a tau angle [0;1]
inline float tauify(const float rad) {
    return rad / (2 * PI);
}

// returns a random number between 0-1
float randf();

// returns a rand number between [0;max[
int randi(int max);

// https://benpfaff.org/writings/clc/shuffle.html
// in-place shuffling of int array
void shuffle(int* array, size_t n);


// lerp between 2 float values
inline float lerpf(const float a, const float b, const float t) {
  return a + (b - a) * t;
}

// lerp between 2 int values
inline int lerpi(const int a, const int b, const float t) {
    return (int)lerpf((float)a, (float)b, t);
}

inline float v_dot(const Point3d a, const Point3d b) {
    return
          a.v[0] * b.v[0]
        + a.v[1] * b.v[1]
        + a.v[2] * b.v[2];
}

void make_v(const Point3d a, Point3d b, Point3d* out);
float v_dot(const Point3d a, const Point3d b);
// returns 1/len
float v_normz(Point3d* a);
void v_cross(const Point3d a, const Point3d b, Point3d* out);
void m_x_v(const Mat4 m, const Point3d v, Point3d* out);
// matrix multiply
void m_x_m(const Mat4  a, const Mat4 b, Mat4 out);
// translate matrix by vector v
void m_x_translate(const Mat4 a, const Point3d v, Mat4 out);
// multiply by y rotation
void m_x_y_rot(const Mat4 a, const float angle, Mat4 out);
// matrix vector multiply invert
// inc.position
void m_inv_x_v(const Mat4 m, const Point3d v, Point3d* out);
// interpolate points
void v_lerp(const Point3d a, const Point3d b, const float t, Point3d* out);

#endif