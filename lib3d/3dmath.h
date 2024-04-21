#ifndef _lib3d_math_h
#define _lib3d_math_h

#define PI 3.1415927410125732421875f

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

// lerp between 2 float values
inline float lerpf(const float a, const float b, const float t) {
  return a + (b - a) * t;
}

void make_v(const Point3d a, Point3d b, Point3d* out);
float v_dot(const Point3d* a, const Point3d* b);
void v_normz(Point3d* a);
void v_cross(const Point3d* a, const Point3d* b, Point3d* out);
void m_x_v(const float* m, const Point3d v,float *out);
void v_lerp(const Point3d* a, const Point3d* b, const float t, Point3d* out);

#endif