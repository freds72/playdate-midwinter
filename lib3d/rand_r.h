#ifndef _lib3d_randr_h
#define _lib3d_randr_h

// initialize seed
void rand_r_init(int seed);

// returns a random number between 0-1
float randf_seeded();

// returns a rand number between [0;max[
int randi_seeded(const int max);

// 
void shuffle_seeded(int* array, const size_t n);

#endif