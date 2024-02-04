#ifndef _lib3d_3dmathi_h
#define _lib3d_3dmathi_h

#include <stdint.h>

typedef struct {
    union{
        struct {
            int x;
            int y;
        };
        int v[2];
    };
} Point2di;

typedef struct {
  union {
    struct {
      int16_t x;
      int16_t y;
      int16_t z;
      int16_t w;
    };
    struct {
      uint32_t v[2];
    };    
  };
} Point3di;

typedef struct {
  union {
    struct {
      uint16_t m[16];
    };
    struct {
      uint32_t v[8];
    };
  };
} Matrix4x4i;

uint32_t v_doti(const Point3di* a,const Point3di* b);
void m_x_vi(const Matrix4x4i* m,const Point3di* p,Point3di* out);

#endif
