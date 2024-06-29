#ifndef _tracks_h
#define _tracks_h

#include <pd_api.h>
#include "ground.h"

#define MAX_TIMELINES 8

typedef struct {
  int ttl;
  int trick_ttl;
  // trick types:
	// 0: slope
	// 1: hole
  int trick_type;
} TrackTimers;

typedef struct {
  int age;
  int is_main;
  int is_dead;
  int width;
  // track extent (absolute)
  int imin;
  int imax;
  // slope positions
  float x;
  float h;
  // dx
  float u;
  TrackTimers timers;
} Track;

typedef struct {
  int xmin;
  int xmax;
  int max_tracks;
  float twist;
  Track tracks[3];
  // active tracks
  int n;
  char pattern[GROUND_WIDTH+1];
} Tracks;

void make_tracks(const int xmin, const int xmax, GroundParams params, Tracks** out);
void update_tracks(int warmup);
void tracks_init(PlaydateAPI* playdate);

#endif
