#include "drawables.h"

static PlaydateAPI* pd;

// indexes all things that are going to be drawn

typedef struct {
    int n;
    // arbitrary limit
    Drawable all[1024];
} Drawables;

typedef struct {
    union {
        struct {
            // low bits
            uint16_t i;
            // high bits (???)
            uint16_t key;
        };
        uint32_t v;
    };
} Sortable;

static Drawables _drawables = {0};
static Sortable _sortables[MAX_DRAWABLES] = {0};

// compare 2 drawables
static int cmp_sortable(const void* a, const void* b) {
    const uint32_t x = ((Sortable*)a)->v;
    const uint32_t y = ((Sortable*)b)->v;
    return x > y ? -1 : x == y ? 0 : 1;
}

void reset_drawables() {
    _drawables.n = 0;
}

Drawable* pop_drawable(const float sortkey) {
    const int i = _drawables.n++;
    if (_drawables.n >= MAX_DRAWABLES) {
        pd->system->error("Drawable pool exhausted: %i/%i", _drawables.n, MAX_DRAWABLES);
        return NULL;
    }
    // pack everything into a int32    
    _sortables[i] = (Sortable){ .i = i, .key = (uint16_t)(max(0.f,sortkey) * 256.0f) };
    return &_drawables.all[i];
}

void draw_drawables(uint8_t* bitmap) {
    if (_drawables.n > 0) {
        qsort(_sortables, (size_t)_drawables.n, sizeof(Sortable), cmp_sortable);

        // rendering
        const int n = _drawables.n;
        for (int k = 0; k < n; k++) {
            Drawable* drawable = &_drawables.all[_sortables[k].i];
            drawable->draw(drawable, bitmap);
        }        
    }
}

void drawables_init(PlaydateAPI* playdate) {
    pd = playdate;
}
