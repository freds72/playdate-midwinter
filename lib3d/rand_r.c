#include <stdlib.h>
#include "rand_r.h"

// single context (no thread here!)
static int _seed;

void rand_r_init(int seed) {
  _seed = seed;
}

int rand_r() {
    _seed = _seed * 1103515245 + 12345;
    return((unsigned)(_seed / 65536) & RAND_MAX);
}

// returns a random number between [0-1[
float randf_r() {
  return rand_r() / 32768.f;
}

// returns a rand number between [0;max[
int randi_r(const int max) {
  return rand_r() % max;
}

void shuffle_r(int* array, const size_t n)
{
    if (n > 1) {
        for (size_t i = 0; i < n - 1; i++) {
            const size_t j = i + rand_r() / (RAND_MAX / (n - i) + 1);
            const int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}