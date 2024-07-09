#include <stdlib.h>
#include "rand_r.h"

// single context (no thread here!)
static int _seed;

void rand_r_init(int seed) {
  _seed = seed;
}

static int rand_next(int* seed) {
    *seed = *seed * 1103515245 + 12345;
    return (*seed / 65536) & 32767;
}

// returns a random number between [0-1[
float randf_seeded() {
  return rand_next(&_seed) / 32768.f;
}

// returns a rand number between [0;max[
int randi_seeded(const int max) {
    return rand_next(&_seed) % max;
}

void shuffle_seeded(int* array, const size_t n)
{
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            const size_t j = i + rand_next(&_seed) / (RAND_MAX / (n - i) + 1);
            const int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}