#ifndef _tracks_h
#define _tracks_h

#include <pd_api.h>

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
  float x;
  float h;
  float u;
  TrackTimers timers;
} Track;

typedef struct {
  int xmin;
  int xmax;
  int max_tracks;
  Track tracks[3];
  // active tracks
  int n;
} Tracks;

void make_tracks(const int xmin, const int xmax, const int max_tracks, Tracks** out);
void update_tracks();
void tracks_init(PlaydateAPI* playdate);

#endif
