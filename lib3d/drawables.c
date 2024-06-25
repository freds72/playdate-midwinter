#include "drawables.h"
#include "spall.h"

static PlaydateAPI* pd;

// indexes all things that are going to be drawn

typedef struct {
    int n;
    // arbitrary limit
    Drawable all[MAX_DRAWABLES];
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

static Drawables _drawables;
static Sortable _sortables[MAX_DRAWABLES];

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
    // pack everything into a int32    
    _sortables[i] = (Sortable){ .i = i, .key = (uint16_t)(max(0.f,sortkey) * 256.0f) };
    return &_drawables.all[i];
}

void draw_drawables(uint8_t* bitmap) {
    BEGIN_BLOCK("draw_drawables");
    if (_drawables.n > 0) {
        BEGIN_BLOCK("qsort");
        qsort(_sortables, (size_t)_drawables.n, sizeof(Sortable), cmp_sortable);
        END_BLOCK();

        // rendering
        const int n = _drawables.n;
        for (int k = 0; k < n; k++) {
            Drawable* drawable = &_drawables.all[_sortables[k].i];
            drawable->draw(drawable, bitmap);
        }
    }
    END_BLOCK();
}

void drawables_init(PlaydateAPI* playdate) {
    pd = playdate;
}
