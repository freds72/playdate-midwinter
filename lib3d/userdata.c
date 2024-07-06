#include "userdata.h"

static PlaydateAPI* pd;

typedef struct {
    const char* id;
    int size;
    int stride;
    int cursor;
    float** ptr;
} FloatPool;

#define FLOAT_POOL(name, st, len) \
static float name ## _values[st * len]; \
static float* name ## _ptr[len]; \
static FloatPool name = { .id = #name, .cursor = 0, .size = len, .stride=st, .ptr = name ## _ptr };

FLOAT_POOL(vec3_pool, VEC3, 2048)
FLOAT_POOL(mat4_pool, MAT4x4, 512)

// helper functions
static void pool_init(FloatPool* pool, float* buffer) {
    float* src = buffer;
    for (int i = 0; i < pool->size; i++, src += pool->stride) {
        pool->ptr[i] = src;
    }
}

static float* pop_float(FloatPool* pool) {
    if (pool->cursor >= pool->size) {
        pd->system->error("Pool: %s exhausted",pool->id);
        return NULL;
    }
    return pool->ptr[pool->cursor++];
}

static void push_float(FloatPool* pool, float* p) {
    if (pool->cursor <= 0) {
        pd->system->error("Pool: %s underflow", pool->id);
    }
    pool->ptr[--pool->cursor] = p;    
}

// api
Point3d* pop_vec3() {
    return (Point3d*)pop_float(&vec3_pool);
}

void push_vec3(Point3d* p) {
    push_float(&vec3_pool, (float*)p);
}

Mat4* pop_mat4() {
    return (Mat4*)pop_float(&mat4_pool);
}

void push_mat4(Mat4* p) {
    push_float(&mat4_pool, (float*)p);
}

// init module
void userdata_init(PlaydateAPI* playdate) {
    pd = playdate;
    // attach pool to poointers
    pool_init(&vec3_pool, vec3_pool_values);
    pool_init(&mat4_pool, mat4_pool_values);
}

void userdata_stats(int* vlen, int* vmax, int* mlen,int *mmax) {
    *vlen = vec3_pool.cursor;
    *vmax = vec3_pool.size;

    *mlen = mat4_pool.cursor;
    *mmax = mat4_pool.size;
}
