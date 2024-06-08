#include "particles.h"
#include "drawables.h"
#include "ground_limits.h"
#include "gfx.h"
#include "spall.h"

static PlaydateAPI* pd;

// shared dither gradients
extern uint32_t _dithers[32 * 16];

// max number of particles
#define PARTICLE_RING_SIZE 128
#define PARTICLE_RING_MOD_SIZE (PARTICLE_RING_SIZE-1)

// check against ground?
#define PARTICLE_SOLID 1

// ring buffer for particles
typedef struct {
    int head;
    int tail;
    Particle particles[PARTICLE_RING_SIZE];
} ParticlesPool;

typedef struct {
    int flags;
    IntRange ttl;
    FloatRange angularv;
    FloatRange decay;
    FloatRange radius;
    FloatRange y_velocity;
    float gravity;
    ParticlesPool pool;
} Emitter;

Emitter _emitters[] = {
    // snow trail
    {
        .flags = 0,
        .ttl = {.min = 24, .max = 45 },
        .angularv = {.min = -1.f, .max=1.f},
        .decay = {.min = 0.95f, .max = 0.97f },
        .radius = {.min = 0.5f, .max = 1.f},
        .y_velocity = {.min = 0.1f, .max = 0.25f},
        .gravity = -0.05f,
        .pool = {0}
    }
};

// activate a new particle from emitter id
void spawn_particle(int id, Point3d pos) {
    Emitter* emitter = &_emitters[id];
    ParticlesPool* pool = &emitter->pool;
    Particle* p = &pool->particles[pool->tail++];
    // init particle
    p->age = emitter->ttl.max;
    p->ttl = lerpi(emitter->ttl.min, emitter->ttl.max, randf());
    p->radius_decay = lerpf(emitter->decay.min, emitter->decay.max, randf());
    p->radius = lerpf(emitter->radius.min, emitter->radius.max, randf());
    p->angle = randf();
    p->angularv = lerpf(emitter->angularv.min, emitter->angularv.max, randf());
    p->pos = pos;
    p->y_velocity = lerpf(emitter->y_velocity.min, emitter->y_velocity.max, randf());

    // loop
    pool->tail &= PARTICLE_RING_MOD_SIZE;
    if (pool->tail == pool->head) {
        pool->head++;
        pool->head &= PARTICLE_RING_MOD_SIZE;
    }
}

// update all particles
void update_particles(Point3d offset) {
    // go over all emitters
    for(int i=0;i<sizeof(_emitters)/sizeof(Emitter);i++) {
        Emitter* emitter = &_emitters[i];
        ParticlesPool* pool = &emitter->pool;
        int it = pool->head;
        while (it != pool->tail) {
            Particle* p = &pool->particles[it++];
            it &= PARTICLE_RING_MOD_SIZE;
            if (p->age--) {
                p->ttl--;
                p->radius *= p->radius_decay;
                p->angle += p->angularv;
                p->y_velocity += emitter->gravity;
                p->pos.y += p->y_velocity;

                // accumulate world offsets
                for (int j = 0; j < 3; j++) p->pos.v[j] += offset.v[j];
            }
            else {
                // dead particle move to next
                pool->head = it;
            }
        }
    }
}

// particles API - rendering

static void draw_particle(Drawable* drawable, uint8_t* bitmap) {
    BEGIN_FUNC();
    DrawableParticle* particle = &drawable->particle;

    // project particle center
    const float w = 1.f / particle->pos.z;
    const float x = 199.5f + 199.5f * w * particle->pos.x;
    const float y = 119.5f - 199.5f * w * particle->pos.y;
    const float radius = 199.5f * particle->radius * w;

    // quad
    Point2d u = { .x = radius * cosf(particle->angle), .y = radius * sinf(particle->angle) };
    Point3du pts[4];
    for (int i = 0; i < 4; i++) {
        pts[i].x = x + u.x;
        pts[i].y = y + u.y;
        // orthogonal next
        const float ux = -u.x;
        u.x = u.y;
        u.y = ux;
    }

    alphafill(pts, 4, 0xffffffff, _dithers + 32 * particle->material, (uint32_t*)bitmap);
    END_FUNC();
}

void push_particles(Drawables* drawables, Point3d cam_pos, float* m, const float y_offset) {
    for(int i=0;i<sizeof(_emitters)/sizeof(Emitter);i++) {
        Emitter* emitter = &_emitters[i];
        ParticlesPool* pool = &emitter->pool;
        int it = pool->head;
        Point3d res;
        while (it != pool->tail) {
            Particle* p = &pool->particles[it++];
            it &= PARTICLE_RING_MOD_SIZE;
            // active?
            if (p->ttl > 0) {
                Point3d tmp = p->pos;
                tmp.y -= y_offset;
                m_x_v(m, tmp.v, res.v);
                // visible?
                if (res.z > Z_NEAR && res.z < (float)(GROUND_CELL_SIZE * MAX_TILE_DIST)) {
                    Drawable* drawable = &drawables->all[drawables->n++];
                    drawable->draw = draw_particle;
                    drawable->key = res.z;
                    drawable->particle.pos = res;
                    drawable->particle.angle = p->angle;
                    drawable->particle.radius = p->radius;
                    drawable->particle.material = (8 * p->age) / emitter->ttl.max;
                    if (drawable->particle.material > 8) drawable->particle.material = 8;
                }
            }
        }
    }
}

// reset all particle pools
void clear_particles() {
    for (int i = 0; i < sizeof(_emitters) / sizeof(Emitter); i++) {
        Emitter* emitter = &_emitters[i];
        ParticlesPool* pool = &emitter->pool;
        pool->head = pool->tail = 0;
    }
}

void particles_init(PlaydateAPI* playdate) {
    pd = playdate;
}