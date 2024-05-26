#ifndef _lib3d_math_h
#define _lib3d_math_h

#define PI 3.1415927410125732421875f
#define MAT4x4 16
#define VEC3 3

// misc. math typedefs
typedef struct {
  union {
    // named access
    struct {
      float x;
      float y;
      float z;
    };
    // values array
    float v[3];
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
    union {
        // named access
        struct {
            float x;
            float y;
            float z;
        };
        // values array
        float v[3];
    };
    float u;
    float light;
} Point3du;

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

void make_v(const Point3d a, Point3d b, Point3d* out);
float v_dot(const float* a, const float* b);
void v_normz(float* a);
void v_cross(const float* a, const float* b, float* out);
void m_x_v(const float* restrict m, const float* restrict v, float* restrict out);
// matrix multiply
void m_x_m(const float* restrict a, const float* restrict b, float* restrict out);
// translate matrix by vector v
void m_x_translate(const float* restrict a, const float* restrict v, float* restrict out);
// matrix vector multiply invert
// inc.position
void m_inv_x_v(const float* restrict m, const float* restrict v, float* restrict out);
void v_lerp(const Point3d* a, const Point3d* b, const float t, Point3d* out);

#endif