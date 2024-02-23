
#ifndef _lib3d_simd_h
#define _lib3d_simd_h

#include <stdint.h>

typedef int32_t q31_t;

static inline uint32_t swap(uint32_t n)
{
#if TARGET_PLAYDATE
    //return __REV(n);
    uint32_t result;

    __asm volatile ("rev %0, %1" : "=l" (result) : "l" (n));
    return(result);
#else
    return ((n & 0xff000000) >> 24) | ((n & 0xff0000) >> 8) | ((n & 0xff00) << 8) | (n << 24);
#endif
}

static inline uint32_t __SADD16(uint32_t op1, uint32_t op2)
{
#if TARGET_PLAYDATE
    uint32_t result;

    __asm volatile ("sadd16 %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
    return(result);
#else
  return  
    (uint32_t)((int16_t)(op1&0xffff) + ((int16_t)(op2&0xffff))) |
    ((uint32_t)(((int16_t)(op1>>8) + (int16_t)(op2>>8))))<<8;
#endif
}

static inline uint32_t __SMLAD(uint32_t x, uint32_t y, uint32_t sum)
{
#if TARGET_PLAYDATE
  uint32_t result;

  __asm volatile ("smlad %0, %1, %2, %3" : "=r" (result) : "r" (x), "r" (y), "r" (sum) );
  return(result);
#else
 return ((uint32_t)(((((q31_t)x << 16) >> 16) * (((q31_t)y << 16) >> 16)) +
                       ((((q31_t)x      ) >> 16) * (((q31_t)y      ) >> 16)) +
                       ( ((q31_t)sum    )                                  )   ));
#endif
}

// convert the given float into a 16:16 fixed point
static inline int32_t __TOFIXED16(float x)
{
    // will corectly generate a vcvt asm instruction
    return (int32_t)(x * (1<<16));
}

#endif
