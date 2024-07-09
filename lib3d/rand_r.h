#ifndef _lib3d_randr_h
#define _lib3d_randr_h

// initialize seed
void rand_r_init(int seed);

// context-aware pseudo-random generator
// ref: https://stackoverflow.com/questions/35231314/random-function-in-multi-threaded-c-program
int rand_r();

// returns a random number between 0-1
float randf_r();

// returns a rand number between [0;max[
int randi_r(const int max);

// 
void shuffle_r(int* array, const size_t n);

#endif