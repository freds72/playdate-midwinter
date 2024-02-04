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

inline float lerpf(const float a, const float b, const float t) {
  return a*(1.f-t)+b*t;
}

void make_v(const Point3d a, Point3d b, Point3d* out);
float v_dot(const Point3d* a, const Point3d* b);
void v_normz(Point3d* a);
void v_cross(const Point3d* a, const Point3d* b, Point3d* out);
void m_x_v(const float* m, const Point3d v,Point3d *out);

#endif