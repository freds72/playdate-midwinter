#include <pd_api.h>
#include <limits.h>
#include "tracks.h"
#include "3dmath.h"


static PlaydateAPI* pd;

static Tracks _tracks;

// game director
typedef struct {
    size_t len;
    char* timeline;
} Timeline;

typedef struct {
    // random placement?
    int random;
    // time in sequence unit
    IntRange seq;
    // section width (computed)
    int width;
    Timeline timelines[MAX_TIMELINES];
} Section;

Section _chill_sections[] = {
    // cabins
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "t" },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // hot air balloon
    {
        .random = 1,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "h" },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // hot air balloon
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "h.." },
            {.timeline = "..." },
            {.timeline = "..h." },
            {.timeline = "...." },
            {.timeline = "...." },
            {.timeline = NULL }
        }
    },
    // eagle(s)
    {
        .random = 1,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "a" },
            {.timeline = NULL }
        }
    },
    // helo
    {
        .random = 1,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "e" },
            {.timeline = NULL }
        }
    },
    // UFO (super rare)
    {
        .random = 1,
        .seq = {.min = 12, .max = 18 },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "u" },
            {.timeline = NULL }
        }
    },
    // cows
    {
        .random = 1,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "M........." },
            {.timeline = "...M......" },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "..M.....M." },
            {.timeline = "....M....." },
            {.timeline = NULL }
        }
    },
    // jump
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "W............" },
            {.timeline = "....12345...."},
            {.timeline = "..J.12345...."},
            {.timeline = "..J.12345...."},
            {.timeline = "....12345...."},
            {.timeline = "W............" },
            {.timeline = NULL }
        }
    }
};

// red track
Section _endless_sections[] = {
    // narrow pass
    {
        .random = 0,
        .seq = {.min = 0, .max = 25 },
        .timelines = {
            {.timeline = "....." },
            {.timeline = "....." },
            {.timeline = "....." },
            {.timeline = NULL }
        }
    },
    // extra narrow pass
    {
        .random = 0,
        .seq = {.min = 8, .max = 25 },
        .timelines = {
            {.timeline = "..C..." },
            {.timeline = NULL }
        }
    },
    // coins 
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "C.C......" },
            {.timeline = "...C.C..." },
            {.timeline = "......C.C" },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // coins 
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "......C.C" },
            {.timeline = "...C.C..." },
            {.timeline = "C.C......" },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // 
    // 1 rock
    {
        .random = 1,
        .seq = {.min = 5, .max = 30 },
        .timelines = {
            { .timeline = "....." },
            { .timeline = "W...R" },
            { .timeline = NULL }
        }
    },
    // 
    // 2x rock
    {
        .random = 1,
        .seq = {.min = 5, .max = 30 },
        .timelines = {
            {.timeline = "........" },
            {.timeline = "W...R..C" },
            {.timeline = "........" },
            {.timeline = "W...R..." },
            {.timeline = NULL }
        }
    },
    // 1 tree
    {
        .random = 1,
        .seq = {.min = 5, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "T" },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // 1 tree + coin
    {
        .random = 1,
        .seq = {.min = 5, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = ".T..C." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // forest 1
    {
        .random = 0,
        .seq = {.min = 10, .max = INT_MAX },
        .timelines = {
            {.timeline = "T..TT.T.T.." },
            {.timeline = "T....CC.D.T.T.T.T" },
            {.timeline = "........CC.D.T.T." },
            {.timeline = ".....G.T.T..CC..." },
            {.timeline = "G.T.....T..T.T.T." },
            {.timeline = NULL }
        }
    },
    // forest 2
    {
        .random = 0,
        .seq = {.min = 10, .max = INT_MAX },
        .timelines = {
            {.timeline = "T..TT.T.T.....C.C" },
            {.timeline = "D.T.T.......G.T.T" },
            {.timeline = "........G.T.T.TT." },
            {.timeline = ".....G.T.T......." },
            {.timeline = "G.T.....T..T.T.T." },
            {.timeline = NULL }
        }
    },
    // forest 3
    {
        .random = 0,
        .seq = {.min = 10, .max = INT_MAX },
        .timelines = {
            {.timeline = "D.T.....T..T.T.T..." },
            {.timeline = ".....D.T.T........." },
            {.timeline = "........D.T.T.TT..." },
            {.timeline = "G.T.T.......D.T.T.." },
            {.timeline = "T..TT.T.T.....C.C.." },
            {.timeline = NULL }
        }
    },
    // forest 3
    {
        .random = 0,
        .seq = {.min = 8, .max = 35 },
        .timelines = {
            {.timeline = "T...D.T..T.T....C." },
            {.timeline = "..............W.T." },
            {.timeline = "T...G.T.T.......C." },
            {.timeline = "T.T.TT.." },
            {.timeline = NULL }
        }
    },
    // 3x rocks
    {
        .random = 0,
        .seq = {.min = 12, .max = 45 },
        .timelines = {
            {.timeline = "."},
            {.timeline = "W....R......"},
            {.timeline = ".W...R..C..C"},
            {.timeline = "W....R......"},
            {.timeline = NULL }
        }
    },
    // 1x snowball
    {
        .random = 1,
        .seq = {.min = 18, .max = 55 },
        .timelines = {
            {.timeline = "."},
            {.timeline = "...B........C.C"},
            {.timeline = NULL }
        }
    },
        // 2x snowball
    {
        .random = 1,
        .seq = {.min = 18, .max = INT_MAX },
        .timelines = {
            {.timeline = ".....B............C...."},
            {.timeline = "."},
            {.timeline = "."},
            {.timeline = ".....B............C...."},
            {.timeline = NULL }
        }
    },
        // 3x snowball
    {
        .random = 1,
        .seq = {.min = 20, .max = INT_MAX },
        .timelines = {
            {.timeline = ".....B............C...."},
            {.timeline = "."},
            {.timeline = ".....B............C...."},
            {.timeline = ".....B............C...."},
            {.timeline = NULL }
        }
    },
    // 3x snowball - no random
    {
        .random = 0,
        .seq = {.min = 20, .max = INT_MAX },
        .timelines = {
            {.timeline = ".....B............C...."},
            {.timeline = "."},
            {.timeline = ".....B............C...."},
            {.timeline = ".....B............C...."},
            {.timeline = NULL }
        }
    }
};

// test
Section _test_sections[] = {
    {
        .random = 0,
        .seq = {.min = 0, .max = INT_MAX },
        .timelines = {
            {.timeline = "..." },
            {.timeline = "..."},
            {.timeline = "..."},
            {.timeline = ".u."},
            {.timeline = "..."},
            {.timeline = "..." },
            {.timeline = NULL }
        }
    }
};

// black track (race)
Section _race_sections[] = {
    // 2x accel pad
    {
        .random = 0,
        .seq = {.min = 1, .max = 12 },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = ".............J." },
            {.timeline = "..............." },
            {.timeline = ".J............." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // 1x accel pad
    {
        .random = 1,
        .seq = {.min = 1, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },  
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "..J.." },
            {.timeline = NULL }
        }
    },
    // coins
    {
        .random = 0,
        .seq = {.min = 4, .max = 12 },
        .timelines = {
            {.timeline = "C..." },
            {.timeline = "...." },
            {.timeline = "...." },
            {.timeline = "..C." },
            {.timeline = "...." },
            {.timeline = "...." },
            {.timeline = "...C" },
            {.timeline = NULL }
        }
    },
    // nothing
    {
        .random = 0,
        .seq = {.min = 8, .max = INT_MAX },
        .timelines = {
            {.timeline = "...." },
            {.timeline = "...." },
            {.timeline = "...." },
            {.timeline = NULL }
        }
    },
    /*
    // accel pad + trees
    {
        .random = 1,
        .seq = {.min = 4, .max = 24 },
        .timelines = {
            {.timeline = ".T...T.T." },
            {.timeline = "." },
            {.timeline = "...T.." },
            {.timeline = "." },
            {.timeline = "T...J" },
            {.timeline = "." },
            {.timeline = "..T.." },
            {.timeline = NULL }
        }
    },
    // accel pad + trees
    {
        .random = 1,
        .seq = {.min = 10, .max = INT_MAX },
        .timelines = {
            {.timeline = ".T" },
            {.timeline = "." },
            {.timeline = ".T." },
            {.timeline = "." },
            {.timeline = "...J." },
            {.timeline = "." },
            {.timeline = "..T.." },
            {.timeline = NULL }
        }
    },
    // 1x accel pad + hazard
    {
        .random = 1,
        .seq = {.min = 4, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = "." },
            {.timeline = "...W..R..J....." },
            {.timeline = "." },
            {.timeline = ".......W..R..J." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    },
    // 1x accel pad + cows
    {
        .random = 1,
        .seq = {.min = 12, .max = INT_MAX },
        .timelines = {
            {.timeline = "." },
            {.timeline = ".....M.." },
            {.timeline = "...M..J....." },
            {.timeline = "." },
            {.timeline = ".......M..J." },
            {.timeline = "." },
            {.timeline = NULL }
        }
    }
    */
};

Section _race_section_start = {
    .random = 0,
    .seq = {.min = 0, .max = 0 },
    .timelines = {
        {.timeline = "W.............." },
        {.timeline = "S.............." },
        {.timeline = "..............." },
        {.timeline = "...n..........m" },
        {.timeline = "..............." },
        {.timeline = "S.............." },
        {.timeline = "W.............." },
        {.timeline = NULL }
    }
};

Section _endless_section_start = {
    .random = 0,
    .seq = {.min = 0, .max = 0 },
    .timelines = {
        {.timeline = "...................." },
        {.timeline = "S..................." },
        {.timeline = "...................." },
        {.timeline = "..................C." },
        {.timeline = "...................." },
        {.timeline = "S..................." },
        {.timeline = "...................." },
        {.timeline = NULL }
    }
};

typedef struct {
    // number of sections
    int n;
    // track sections
    Section* sections;
    // start
    Section* start;
} SectionCatalog;

// all track types
#define CATALOG_ENTRY(s0,s) { .start = s0, .n = sizeof(s) / sizeof(Section), .sections = s }

static SectionCatalog _catalog[] = {
    CATALOG_ENTRY(NULL,_chill_sections),
    CATALOG_ENTRY(&_endless_section_start,_endless_sections),
    CATALOG_ENTRY(&_race_section_start,_race_sections),
    CATALOG_ENTRY(NULL,_test_sections)
};

struct {
    int min_cooldown;
    int max_cooldown;
    // current cooldown value
    int cooldown;
    // next
    int next_cooldown;
    // cooldown value (to compute ratio)
    int total_cooldown;
    // current timeline cursor
    int t;
    // seq count
    int seq;
    // current timeline length
    size_t max_len;
    Timeline* lanes[MAX_TIMELINES];
    SectionCatalog catalog;
    Section* active_section;
    Section* next_section;
} _section_director;

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

  // default values
  new_track->age = 0;
  new_track->h = 0;
  new_track->is_main = is_main;
  new_track->is_dead = 0;
  new_track->x = x;
  new_track->u = u;
  new_track->width = 4;

  return new_track;
}

static Section* pick_next_section() {
    // any "forced" start section?
    if (_section_director.seq == 0 && _section_director.catalog.start) {
        _section_director.seq++;
        return _section_director.catalog.start;
    }

    Section* sections[16] = {NULL};
    int n = 0;
    // pick section
    int seq = _section_director.seq;
    for (int i = 0; i < _section_director.catalog.n; i++) {
        Section* s = &_section_director.catalog.sections[i];
        if (seq >= s->seq.min && seq <= s->seq.max ) {
            sections[n++] = s;
        }
    }
    _section_director.seq++;

    if (!n) {
        pd->system->error("No active section to pick @%i", _section_director.seq);
        return NULL;
    }

    return sections[randi(n)];
}

static int update_track(Track *track,int warmup)
{
  if (track->is_dead)
    return 0;

  if (!warmup && track->is_main) {
      // next section?
      if (!_section_director.active_section) {
          // lerp width
          int w0 = _section_director.active_section ? _section_director.active_section->width : 4;
          int w1 = _section_director.next_section ? _section_director.next_section->width : 4;
          track->width = lerpi(w0, w1, (float)(_section_director.total_cooldown - _section_director.cooldown) / _section_director.total_cooldown);

          _section_director.cooldown--;

          if (_section_director.cooldown < 0) {
              // pick a random section (handle init case) 
              _section_director.active_section = _section_director.next_section ? _section_director.next_section : pick_next_section();
              _section_director.next_section = pick_next_section();

              // no available section?
              if (_section_director.active_section) {
                  _section_director.t = 0;
                  Section* s = _section_director.active_section;
                  // pick random lanes
                  int lanes[] = { 0,1,2,3,4,5,6,7 };
                  if (s->random)
                      shuffle(lanes, s->width); // shuffle only within the effective timelines (eg. no "blanks")
                  memset(_section_director.lanes, 0, sizeof(Timeline*) * MAX_TIMELINES);
                  int j = 0;
                  size_t max_len = 0;
                  while (s->timelines[j].timeline) {
                      Timeline* timeline = &s->timelines[j];
                      _section_director.lanes[lanes[j]] = timeline;
                      if (timeline->len > max_len) max_len = timeline->len;
                      j++;
                  }
                  _section_director.max_len = max_len;
              }
          }
      }

      if (_section_director.active_section) {
          track->width = _section_director.active_section->width;

          memset(_tracks.pattern, ' ', GROUND_WIDTH);
          if (_section_director.t < _section_director.max_len) {
              const int ii = (int)(track->x / GROUND_CELL_SIZE);
              track->imin = ii - track->width / 2;
              track->imax = ii + track->width / 2;

              int t = _section_director.t;
              for (int i = 0; i < MAX_TIMELINES; ++i) {
                  Timeline* timeline = _section_director.lanes[i];
                  // active?
                  if (timeline) {
                      _tracks.pattern[track->imin + i] = timeline->timeline[t % timeline->len];
                  }
              }
              _section_director.t++;

              // don't twist sections!
              return 1;
          }
          else {
              _section_director.active_section = NULL;
              _section_director.cooldown = _section_director.next_cooldown;

              // select next section
              _section_director.total_cooldown = lerpi(_section_director.min_cooldown, _section_director.max_cooldown, randf());
              _section_director.next_cooldown = _section_director.total_cooldown;
          }
      }
  }

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
      track->timers.ttl = 4 + (int)(2.f * randf());
    }
  }
  if (track->timers.ttl < 0)
  {
    // reset
    reset_track_timers(&track->timers, 0);
    track->u = _tracks.twist * (1.6f * randf() - 0.8f);
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

  // track extent (in tile units)
  const int ii = (int)(track->x / GROUND_CELL_SIZE);
  track->imin = ii - track->width / 2;
  track->imax = ii + track->width / 2;

  return 1;
}

// generate ski tracks
// xmin/xmax: min/max world coordinates for track
void make_tracks(const int xmin, const int xmax, GroundParams params, Tracks **out)
{
  float angle = 0.25f + 0.45f * randf();

  // set global range
  _tracks.xmin = xmin;
  _tracks.xmax = xmax;
  _tracks.max_tracks = params.num_tracks;
  _tracks.n = 0;
  _tracks.twist = params.twist;
  memset(_tracks.pattern, ' ', GROUND_WIDTH);
  _tracks.pattern[GROUND_WIDTH] = 0;

  // section director
  _section_director.seq = 0;
  _section_director.active_section = NULL;
  _section_director.next_section = NULL;
  _section_director.min_cooldown = params.min_cooldown;
  _section_director.max_cooldown = params.max_cooldown;  
  // debug safeguards
#ifdef _DEBUG
  if (params.min_cooldown <= 0 || params.min_cooldown > params.max_cooldown) {
      pd->system->error("Invalid cooldown values: %i/%i", params.min_cooldown, params.max_cooldown);
  }
#endif 
  _section_director.cooldown = lerpi(params.min_cooldown, params.max_cooldown, randf());
  _section_director.next_cooldown = lerpi(params.min_cooldown, params.max_cooldown, randf());
  _section_director.catalog = _catalog[params.track_type];

  add_track(lerpf(xmin, xmax, randf()), cosf(detauify(angle)), 1);

  *out = &_tracks;
}

void update_tracks(int warmup)
{
  int i = 0;
  while (i < _tracks.n)
  {
    if (!update_track(&_tracks.tracks[i], warmup))
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

static void init_section(Section* s, int i) {
    int j = 0;
    // cache length of string
    while (s->timelines[j].timeline) {
        s->timelines[j].len = strlen(s->timelines[j].timeline);
        j++;
    }
    s->width = j;
    if (j >= MAX_TIMELINES)
        pd->system->error("Too many timelines on section: %i - %i/%i", i, j, MAX_TIMELINES);
}

// init module
void tracks_init(PlaydateAPI* playdate) {
    pd = playdate;

    _section_director.active_section = NULL;
    _section_director.next_section = NULL;
    _section_director.total_cooldown = 24;    
    _section_director.cooldown = _section_director.total_cooldown;

    // initialize timeline string lenghts
    for (int k = 0; k < sizeof(_catalog) / sizeof(SectionCatalog); ++k) {
        SectionCatalog* catalog = &_catalog[k];
        if (catalog->start) init_section(catalog->start, -1);
        for (int i = 0; i < catalog->n; ++i) {
            init_section(&catalog->sections[i], i);
        }
    }
}