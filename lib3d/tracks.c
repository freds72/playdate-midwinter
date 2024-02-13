#include <pd_api.h>
#include "tracks.h"
#include "3dmath.h"


static PlaydateAPI* pd;

static Tracks _tracks;

static void reset_track_timers(TrackTimers *timers)
{
  timers->ttl = 12 + 20 * randf();
  timers->trick_ttl = 4 + 4 * randf();
  timers->trick_type = randf() > 0.5 ? 0 : 1;
}

static Track *add_track(const int x, const float u, int is_main)
{
  Track *new_track = &_tracks.tracks[_tracks.n++];

  reset_track_timers(&new_track->timers);

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
    reset_track_timers(&track->timers);
    track->u = cosf(detauify(0.05f + 0.45f * randf()));
    // offshoot?
    if (randf() < 0.5f && _tracks.n < _tracks.max_tracks - 1)
    {
      add_track(track->x, -track->u, 0);
    }
  }
  track->x += track->u;
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
void make_tracks(const int xmin, const int xmax, const int max_tracks, Tracks **out)
{
  float angle = 0.05f + 0.45f * randf();

  // set global range
  _tracks.xmin = xmin;
  _tracks.xmax = xmax;
  _tracks.max_tracks = max_tracks;
  _tracks.n = 0;
  
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
  char buffer[33];
  memset(buffer, 32, 33);
  buffer[0] = '|';
  buffer[31] = '|';  
  for(int i = 0; i < _tracks.n; i++) {
      Track* t = &_tracks.tracks[i];
      buffer[(int)(t->x / 4)] = t->is_main ? '*' : '$';
  }
  pd->system->logToConsole(buffer);
}

// init module
void tracks_init(PlaydateAPI* playdate) {
    pd = playdate;
}