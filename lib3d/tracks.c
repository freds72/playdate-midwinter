#include <pd_api.h>
#include "tracks.h"
#include "3dmath.h"


static PlaydateAPI* pd;

static Tracks _tracks;

static void reset_track_timers(TrackTimers *timers, int is_main)
{
  timers->ttl = 12 + (int)(8.f * randf());
  // make sure coins are not spawned at start
  timers->trick_ttl = is_main?60 + (int)(15.f * randf()): 8 + (int)(4.f * randf());
  timers->trick_type = randf() > 0.5f;
}

static Track *add_track(const float x, const float u, int is_main)
{
  Track *new_track = &_tracks.tracks[_tracks.n++];

  reset_track_timers(&new_track->timers, is_main); 

  new_track->age = 0;
  new_track->h = 0;
  new_track->is_main = is_main;
  new_track->is_dead = 0;
  new_track->x = x;
  new_track->u = u;
  return new_track;
}

static int update_track(Track *track)
{
  if (track->is_dead)
    return 0;
  track->age++;
  track->timers.trick_ttl--;
  track->timers.ttl--;
  if (!track->is_main && track->timers.trick_ttl < 0)
  {
    switch (track->timers.trick_type)
    {
    case 0:
      track->h += 1.5f;
      break;
    case 1:
      track->h = -4;
      break;
    }
    // not too high + ensure straight line
    if (track->timers.trick_ttl < -5)
    {
      track->h = 0;
      track->timers.ttl = 4 + 2 * randf();
    }
  }
  if (track->timers.ttl < 0)
  {
    // reset
    reset_track_timers(&track->timers, 0);
    track->u = _tracks.twist * sinf(detauify(0.45f * randf()));
    // offshoot?
    if (_tracks.n < _tracks.max_tracks && randf() < 0.25f)
    {
        add_track(track->x, -track->u, 0);
    }
  }
  track->x +=track->u;
  if (track->x < _tracks.xmin)
  {
    track->x = _tracks.xmin;
    track->u = -track->u;
  }
  if (track->x > _tracks.xmax)
  {
    track->x = _tracks.xmax;
    track->u = -track->u;
  }

  return 1;
}

// generate ski tracks
// xmin/xmax: min/max world coordinates for track
void make_tracks(const int xmin, const int xmax, const int max_tracks, const float twist, Tracks **out)
{
  float angle = 0.25f + 0.45f * randf();

  // set global range
  _tracks.xmin = xmin;
  _tracks.xmax = xmax;
  _tracks.max_tracks = max_tracks;
  _tracks.n = 0;
  _tracks.twist = twist;
  
  add_track(lerpf(xmin, xmax, randf()), cosf(detauify(angle)), 1);

  *out = &_tracks;
}

void update_tracks()
{
  int i = 0;
  while (i < _tracks.n)
  {
    if (!update_track(&_tracks.tracks[i]))
    {
      // shift other tracks down
      for (int j = i + 1; j < _tracks.n; j++)
      {
        _tracks.tracks[j - 1] = _tracks.tracks[j];
      }
      _tracks.n--;
    }
    else
    {
      i++;
    }
  }

  // kill intersections
  for (int i = 0; i < _tracks.n; i++)
  {
    Track *s0 = &_tracks.tracks[i];
    for (int j = i + 1; j < _tracks.n; j++)
    {
      Track *s1 = &_tracks.tracks[j];
      // don't kill new seeds
      // don't kill is_main track
      if (s1->age > 0 && (int)(s0->x - s1->x) == 0)
      {
        // don'kill main track
        s1 = s1->is_main ? s0 : s1;
        s1->is_dead = 1;
      }
    }
  }

  // debug dump track outline
  /*
  char buffer[33];
  memset(buffer, 32, 32);
  buffer[0] = '|';
  buffer[31] = '|';  
  buffer[32] = 0;
  for(int i = 0; i < _tracks.n; i++) {
      Track* t = &_tracks.tracks[i];
      buffer[(int)(t->x / 4)] = t->is_main ? '*' : '$';
  }
  pd->system->logToConsole("%s [%i %i]",buffer,_tracks.xmin,_tracks.xmax);
  */  
}

// init module
void tracks_init(PlaydateAPI* playdate) {
    pd = playdate;
}