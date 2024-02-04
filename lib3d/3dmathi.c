#include "3dmathi.h"
#include "simd.h"

uint32_t v_doti(const Point3di* a,const Point3di* b) {
  uint32_t sum = 0;
  sum = __SMLAD(a->v[0],b->v[0], sum);
  sum = __SMLAD(a->v[1],b->v[1], sum);
  return sum;
}

void m_x_vi(const Matrix4x4i* m,const Point3di* p,Point3di* out) {

}
