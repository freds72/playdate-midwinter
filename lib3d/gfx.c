#include <pd_api.h>
#include <float.h>
#include "simd.h"
#include "gfx.h"


#define LCD_ROWSIZE32 (LCD_ROWSIZE/4)

static inline void _drawMaskPattern(uint32_t* p, uint32_t mask, uint32_t color)
{
    if (mask == 0xffffffff)
        *p = color;
    else
        *p = (*p & ~mask) | (color & mask);
}

static void drawFragment(uint32_t* row, int x1, int x2, uint32_t color)
{
    if (x2 < 0 || x1 >= LCD_COLUMNS)
        return;

    if (x1 < 0)
        x1 = 0;

    if (x2 > LCD_COLUMNS)
        x2 = LCD_COLUMNS;

    if (x1 > x2)
        return;

    // Operate on 32 bits at a time

    int startbit = x1 & 31;
    uint32_t startmask = swap((1 << (32 - startbit)) - 1);
    int endbit = x2 & 31;
    uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));

    int col = x1 / 32;
    uint32_t* p = row + col;

    if (col == x2 / 32)
    {
        uint32_t mask = 0;

        if (startbit > 0 && endbit > 0)
            mask = startmask & endmask;
        else if (startbit > 0)
            mask = startmask;
        else if (endbit > 0)
            mask = endmask;

        _drawMaskPattern(p, mask, color);
    }
    else
    {
        int x = x1;

        if (startbit > 0)
        {
            _drawMaskPattern(p++, startmask, color);
            x += (32 - startbit);
        }

        while (x + 32 <= x2)
        {
            _drawMaskPattern(p++, 0xffffffff, color);
            x += 32;
        }

        if (endbit > 0)
            _drawMaskPattern(p, endmask, color);
    }
}

#define FIXED_SCALE (1<<8)
static inline int tofixed(const float f) { return (int)(f * FIXED_SCALE); }
static inline float tofloat(const int i) { return ((float)i) / FIXED_SCALE; }

void polyfill(const Point3d* verts, const int n, uint32_t* dither, uint32_t* bitmap) {
	float miny = FLT_MAX, maxy = FLT_MIN;
	int mini = -1;
	// find extent
	for (int i = 0; i < n; ++i) {
		float y = verts[i].y;
		if (y < miny) miny = y, mini = i;
		if (y > maxy) maxy = y;
	}
    // out of screen?
    if (miny > LCD_ROWS) return;
    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny < 0) miny = 0;

	// data for left& right edges :
	int lj = mini, rj = mini;
    int ly = (int)miny, ry = (int)miny;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;
    for (uint8_t y =((int)miny); y < maxy; ++y){
        // maybe update to next vert
        while (ly < y) {
            const Point3d* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3d* p1 = &verts[lj];
            float y0 = p0->y, y1 = p1->y;
            float dy = y1 - y0;
            ly = (int)y1;
            lx = tofixed(p0->x);
            ldx = tofixed((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            lx += (int)(cy * ldx);
        }
        while (ry < y) {
            const Point3d* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3d* p1 = &verts[rj];
            float y0 = p0->y, y1 = p1->y;
            float dy = y1 - y0;
            ry = (int)y1;
            rx = tofixed(p0->x);
            rdx = tofixed((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
        }

        drawFragment(bitmap + y * LCD_ROWSIZE32, lx>>8, rx>>8, dither[y&31]);

        lx += ldx;
        rx += rdx;
    }    
}

// Dimensions of our pixel group
static const int stepXSize = 8;
static const int stepYSize = 1;

typedef struct {
    union {
        int16_t v[8];
        uint32_t word[4];
    };  
} Vec8i;

typedef struct{
    Vec8i oneStepX;
    Vec8i oneStepY;
} Edge;

static inline Vec8i make_vec(const int16_t a) {
    return (Vec8i) {
        .v = { 
        a,
        a,
        a,
        a,
        a,
        a,
        a,
        a}
    };
}

static Vec8i edge_init(Edge* edge, const Point2di* v0, const Point2di* v1,const Point2di* origin)
{
    // Edge setup
    int16_t A = v0->y - v1->y, B = v1->x - v0->x;
    int16_t C = v0->x * v1->y - v0->y * v1->x;

    // Step deltas
    edge->oneStepX = make_vec(A * stepXSize);
    edge->oneStepY = make_vec(B * stepYSize);

    // x/y values for initial pixel block
    Vec8i x = (Vec8i) {
        .v = { 
        origin->x + 0, 
        origin->x + 1, 
        origin->x + 2, 
        origin->x + 3,
        origin->x + 4,
        origin->x + 5,
        origin->x + 6,
        origin->x + 7 }
    };
    Vec8i y = make_vec(origin->y);

    // Edge function values at origin
    Vec8i out;
    for (int i = 0; i < 8; ++i) {
        out.v[i] = A * x.v[i] + B * y.v[i] + C;
    }
    return out;
}

static uint8_t vec_mask(const Vec8i* a, const Vec8i* b, const Vec8i* c) {
    uint8_t out = 0;
    for (int i = 0; i < 8; ++i) {
        out |= (a->v[i] | b->v[i] | c->v[i]) >= 0 ? (0x80>>i) : 0;
    }
    return out;
}

static void vec_inc(Vec8i* a, Vec8i* b) {
    for (int i = 0; i < 8; ++i) {
        a->v[i] += b->v[i];
    }
}

static int orient2d(const Point2di* a, const Point2di* b, const Point2di* c)
{
    return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}
static int min3(int a, int b, int c) {
    if (a < b) b = a;
    if (c < b) b = c;
    return b;
}
static int max3(int a, int b, int c) {
    if (a > b) b = a;
    if (c > b) b = c;
    return b;
}

void trifill(const Point2di* v0, const Point2di* v1, const Point2di* v2, uint8_t* bitmap) {
    // Compute triangle bounding box
    int16_t minX = min3(v0->x, v1->x, v2->x);
    int16_t minY = min3(v0->y, v1->y, v2->y);
    int16_t maxX = max3(v0->x, v1->x, v2->x);
    int16_t maxY = max3(v0->y, v1->y, v2->y);

    // Clip against screen bounds
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= LCD_COLUMNS) maxX = LCD_COLUMNS - 1;
    if (maxY >= LCD_ROWS) maxY = LCD_ROWS - 1;

    // align to uint8 boundaries
    minX = 8 * (minX / 8);
    maxX = 8 * (1 + (maxX-1) / 8);

    // Barycentric coordinates at minX/minY corner
    Point2di p = (Point2di){.v = {minX, minY}};
    Edge e01, e12, e20;

    // Triangle setup
    Vec8i w0_row = edge_init(&e12,v1, v2, &p);
    Vec8i w1_row = edge_init(&e20,v2, v0, &p);
    Vec8i w2_row = edge_init(&e01,v0, v1, &p);

    // Rasterize
    for (int y = minY; y <= maxY; y+=stepYSize) {
        uint8_t* bitmap_row = bitmap + y * LCD_ROWSIZE;
        // Barycentric coordinates at start of row
        Vec8i w0 = w0_row;
        Vec8i w1 = w1_row;
        Vec8i w2 = w2_row;
        for (int x = minX; x <= maxX; x+=stepXSize) {
            // If p is on or inside all edges, render pixel.
            uint8_t mask = vec_mask(&w0, &w1, &w2);
            if (mask) {
                bitmap_row[x / 8] = (bitmap_row[x / 8] & ~mask) | mask;
            }
            // One step to the right
            vec_inc(&w0,&e12.oneStepX);
            vec_inc(&w1,&e20.oneStepX);
            vec_inc(&w2,&e01.oneStepX);
        }
        // One row step
        vec_inc(&w0_row, &e12.oneStepY);
        vec_inc(&w1_row, &e20.oneStepY);
        vec_inc(&w2_row, &e01.oneStepY);
    }
}