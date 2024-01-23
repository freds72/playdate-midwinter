//
//  3d-glue.c
//  Extension
//
//  Created by Dave Hayden on 9/19/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include <math.h>
#include <float.h>
#include "luaglue.h"
#include "realloc.h"
#include "perlin.h"

#define fswap(a,b) {float tmp = b; b=a; a=tmp;}
#define SQR(a) a*a

static inline uint32_t swap(uint32_t n)
{
#if TARGET_PLAYDATE
    //return __REV(n);
    uint32_t result;

    __asm volatile ("rev %0, %1" : "=l" (result) : "l" (n));
    return(result);
#else
    return ((n & 0xff000000) >> 24) | ((n & 0xff0000) >> 8) | ((n & 0xff00) << 8) | (n << 24);
#endif
}

#define LCD_ROWSIZE32 (LCD_ROWSIZE/4)

static PlaydateAPI* pd = NULL;

// misc. math typedefs
typedef struct {
    float x;
    float y;
    float z;
} Point3d;

typedef struct {
    float x;
    float y;
} Point2d;

typedef struct {
    Point3d n;
    float cp;
    // 0: tri
    // 1: tri (flipped)
    // 2: quad
    int type;
} Face;

// large 32x32 pattern
typedef uint32_t DitherPattern[32];

DitherPattern _dithers[16];

#define GRID_SIZE 32
float _grid[GRID_SIZE * GRID_SIZE];
Face _grid_strip[GRID_SIZE * GRID_SIZE * 2];

static void make_v(Point3d* a, Point3d* b, Point3d* out) {
    out->x = b->x - a->x;
    out->y = b->y - a->y;
    out->z = b->z - a->z;
}

static void v_normz(Point3d* a) {
    float d = sqrtf(SQR(a->x) + SQR(a->y) + SQR(a->z));
    if (fabs(d) < 0.0001f) return;
    a->x /= d;
    a->y /= d;
    a->z /= d;
}

static void draw_polygon_wireframe(Point2d* verts, int* indices, int n, uint8_t* bitmap) {
    Point2d p0 = verts[indices[n - 1]];
    for (int i = 0; i < n; ++i) {
        Point2d p1 = verts[indices[i]];
        float x0 = p0.x;
        float y0 = p0.y;
        float x1 = p1.x;
        float y1 = p1.y;
        if (fabs(x1 - x0) > fabs(y1 - y0)) {
            if (x0 > x1) {
                fswap(x0, x1);
                fswap(y0, y1);
            }
            float dy = (y1 - y0) / (x1 - x0);
            if (x0 < 0) y0 += x0 * dy, x0 = 0;
            float y = y0 + ((x0-(int)x0)) * dy;
            for (int x = (int)x0; x < min((int)x1,LCD_COLUMNS); ++x)
            {
                int offset = (x / 8) + ((int)y) * LCD_ROWSIZE;
                uint8_t prev = bitmap[offset];
                uint8_t mask = 0x80>>(x&7);
                bitmap[offset] = (prev & ~mask) | mask;
                y += dy;
            }
        }
        else {
            if (y0 > y1) {
                fswap(x0, x1);
                fswap(y0, y1);
            }
            float dx = (x1 - x0) / (y1 - y0);
            if (y0 < 0) x0 += y0 * dx, y0 = 0;
            float x = x0 + ((y0 - (int)y0)) * dx;
            for (int y = (int)y0; y < min((int)y1,LCD_ROWS); ++y)
            {
                int ix = (int)x;
                int offset = (ix / 8) + y * LCD_ROWSIZE;
                uint8_t prev = bitmap[offset];
                uint8_t mask = 0x80 >> (ix & 7);
                bitmap[offset] = (prev & ~mask) | mask;
                x += dx;
            }
        }
        p0 = p1;
    }
}

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

static void draw_polygon(Point2d* verts, int n, uint32_t* bitmap) {
	float miny = FLT_MAX, maxy = FLT_MIN;
	int mini = -1;
	// find extent
	for (int i = 0; i < n; ++i) {
		float y = verts[i].y;
		if (y < miny) miny = y, mini = i;
		if (y > maxy) maxy = y;
	}

	// data for left& right edges :
	int lj = mini, rj = mini;
    int ly = (int)miny, ry = (int)miny;
    float lx = 0, ldx = 0, rx = 0, rdx = 0;
	if (maxy >= LCD_ROWS) maxy = LCD_ROWS-1;
	if (miny < 0) miny = -1;
    for (uint8_t y =((int)miny) + 1; y <= maxy; ++y){
        // maybe update to next vert
        while (ly < y) {
            Point2d p0 = verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            Point2d p1 = verts[lj];
            float y0 = p0.y, y1 = p1.y;
            float dy = y1 - y0;
            ly = (int)y1;
            lx = p0.x;
            ldx = (p1.x - lx) / dy;
            //sub - pixel correction
            float cy = y - y0;
            lx += cy * ldx;
        }
        while (ry < y) {
            Point2d p0 = verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            Point2d p1 = verts[rj];
            float y0 = p0.y, y1 = p1.y;
            float dy = y1 - y0;
            ry = (int)y1;
            rx = p0.x;
            rdx = (p1.x - rx) / dy;
            //sub - pixel correction
            float cy = y - y0;
            rx += cy * rdx;
        }

        drawFragment(bitmap + y * LCD_ROWSIZE32, (int)rx, (int)lx, _dithers[1][y%8]);

        lx += ldx;
        rx += rdx;
    }    
}

// 
static int lib3d_render(lua_State* L)
{
    // get param count
    int len = pd->lua->getArgCount();
    // target matrix
    Point2d* verts = (Point2d*)lib3d_malloc((len/2) * sizeof(Point2d));

    for (int index = 0; index < len; ++index) {
        // Get it's value.        
        float v = pd->lua->getArgFloat(index + 1);
        if (index%2==0)
            verts[index / 2].x = v;
        else
            verts[index / 2].y = v;
    }

    // draw_polygon(verts, len / 2, (uint32_t*)pd->graphics->getFrame());
    uint8_t* bitmap = pd->graphics->getFrame();

    for (int j = 0; j < GRID_SIZE-1; j++) {
        for (int i = 0; i < GRID_SIZE-1; i++) {
            Point3d verts[4] = {
                { i, _grid[i + j * GRID_SIZE], j },
                { i + 1,_grid[i + 1 + j * GRID_SIZE], j },
                { i + 1,_grid[i + 1 + (j + 1) * GRID_SIZE], j + 1 },
                { i,_grid[i + (j + 1) * GRID_SIZE], j + 1 } };
            Point2d pts[4];
            for (int i = 0; i < 4; i++) {
                pts[i].x = verts[i].x * 4;
                pts[i].y = verts[i].z * 4;
            }
            Face f0 = _grid_strip[2 * i + j * GRID_SIZE];
            if (f0.type == 0) {                
                draw_polygon_wireframe(pts, (int[]) { 0, 1, 3 }, 3, bitmap);
                draw_polygon_wireframe(pts, (int[]) { 1, 2, 3 }, 3, bitmap);
            }
        }
    }

    pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

    lib3d_free(verts);

    return 1;
}

void lib3d_register(PlaydateAPI* playdate)
{
	pd = playdate;

	const char* err;

	if ( !pd->lua->addFunction(lib3d_render, "lib3d.render", &err) )
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

    // read dither table
    for (int i = 0; i < 16; ++i) {
        char* path = NULL;
        pd->system->formatString(&path, "images/noise32x32-%d", i);
        LCDBitmap* dither = pd->graphics->loadBitmap(path, &err);
        if (!dither)
            pd->system->logToConsole("Failed to load: %s, %s", path, err);
        int w = 0, h = 0, r = 0;
        uint8_t* mask = NULL;
        uint8_t* data = NULL;
        pd->graphics->getBitmapData(dither, &w, &h, &r, &mask, &data);
        if (w!=32 || h!=32) 
            pd->system->logToConsole("Invalid pattern format: %dx%d, file: %s", w, h, path);
        for (int j = 0; j < 32; ++j) {
            int mask = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
            _dithers[i][j] = mask;
            data += 4;
        }
    }

    // create grid
    for (int j = 0; j < GRID_SIZE; j++) {
        for (int i = 0; i < GRID_SIZE; i++) {
            _grid[i + j * GRID_SIZE] = perlin2d(i, j, 0.1f, 4);
        }
        // fake path
        for (int i = 12; i < 24; i++) {
            _grid[i + j * GRID_SIZE] /= 2;
            if (fabs(_grid[i + j * GRID_SIZE]) < 0.1f)
                _grid[i + j * GRID_SIZE] = 0;
        }
    }

    for (int j = 0; j < GRID_SIZE-1; j++) {
        for (int i = 0; i < GRID_SIZE - 1; i++) {
            Point3d p0 = { i,_grid[i + j * GRID_SIZE], j };
            Point3d p1 = { i + 1,_grid[i + 1 + j * GRID_SIZE], j };
            Point3d p2 = { i + 1,_grid[i + 1 + (j+1) * GRID_SIZE], j+1 };
            Point3d p3 = { i,_grid[i + (j+1) * GRID_SIZE], j+1 };
            _grid_strip[2*i + j * GRID_SIZE] = (Face){.n = (Point3d) {.x=0,.y=0,.z=0}, .cp = 0, .type = 0};
            _grid_strip[2*i+1 + j * GRID_SIZE] = (Face){ .n = (Point3d) {.x = 0,.y = 0,.z = 0}, .cp = 0, .type = 1 };
        }
    }

	lib3d_setRealloc(pd->system->realloc);
}
