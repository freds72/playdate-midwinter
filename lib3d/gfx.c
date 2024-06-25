#include <pd_api.h>
#include <float.h>
#include "simd.h"
#include "spall.h"
#include "gfx.h"


static PlaydateAPI* pd = NULL;

void gfx_init(PlaydateAPI* playdate) {
    pd = playdate;
}

#define LCD_ROWSIZE32 (LCD_ROWSIZE/4)

static inline void _drawMaskPattern(uint32_t* p, uint32_t mask, uint32_t color)
{
    *p = (*p & ~mask) | (color & mask);
}

static inline void _drawMaskPatternOpaque(uint32_t* p, uint32_t color)
{
    *p = color;
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
            _drawMaskPatternOpaque(p++, color);
            x += 32;
        }

        if (endbit > 0)
            _drawMaskPattern(p, endmask, color);
    }
}

static void drawTextureFragment(uint8_t* row, int x1, int x2, int lu, int ru, uint8_t* dither_ramp)
{
    if (x2 < 0 || x1 >= LCD_COLUMNS)
        return;

    int dx = x2 - x1;
    if (dx == 0) return;
    // source is fixed point already
    int du = (ru - lu) / dx;
    if (x1 < 0) {
        lu -= x1 * du;
        x1 = 0;
    }
    
    if (x2 > LCD_COLUMNS)
        x2 = LCD_COLUMNS;

    if (x1 > x2)
        return;

    // Operate on 8 bits at a time

    const int startbit = x1 & 7;
    const uint8_t startmask = (1 << (8 - startbit)) - 1;
    const int endbit = x2 & 7;
    const uint8_t endmask = ((1 << endbit) - 1) << (8 - endbit);

    // ensure lu by step of 8*du do not overlap with invalid noise
    // dither_ramp++;

    // move by whole shading block (2x32 pixels)
    du *= 8;
    
    int col = x1 / 8;
    uint8_t* p = row + col;
    
    if (col == x2 / 8)
    {
        uint8_t mask = 0;

        if (startbit > 0 && endbit > 0)
            mask = startmask & endmask;
        else if (startbit > 0)
            mask = startmask;
        else if (endbit > 0)
            mask = endmask;

        *p = (*p & ~mask) | ((*(dither_ramp + (lu >> 16) * 8 + (col&3))) & mask);
    }
    else
    {
        int x = x1;

        if (startbit > 0)
        {
            *p = (*p & ~startmask) | ((*(dither_ramp + (lu >> 16) * 8 + ((x/8) & 3))) & startmask);
            x += (8 - startbit);
            lu += du;
            p++;
        }

        while (x + 8 <= x2)
        {
            *(p++) = *(dither_ramp + (lu >> 16) * 8 + ((x / 8) & 3));
            lu += du;
            x += 8;
        }

        if (endbit > 0) {
            // the last block of 8 pixels can overflow into negative territory
            if (lu < 0) lu = 0;           
            *p = (*p & ~endmask) | ((*(dither_ramp + (lu >> 16) * 8 + ((x/8) % 4))) & endmask);
        }
    }
}

// todo: http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html

static inline void sortTri(Point3du** p1, Point3du** p2, Point3du** p3)
{
    float y1 = (*p1)->y, y2 = (*p2)->y, y3 = (*p3)->y;

    if (y1 <= y2 && y1 < y3)
    {
        if (y3 < y2) // 1,3,2
        {
            Point3du* tmp = *p2;
            *p2 = *p3;
            *p3 = tmp;
        }
    }
    else if (y2 < y1 && y2 < y3)
    {
        Point3du* tmp = *p1;
        *p1 = *p2;

        if (y3 < y1) // 2,3,1
        {
            *p2 = *p3;
            *p3 = tmp;
        }
        else // 2,1,3
            *p2 = tmp;
    }
    else
    {
        Point3du* tmp = *p1;
        *p1 = *p3;

        if (y1 < y2) // 3,1,2
        {
            *p3 = *p2;
            *p2 = tmp;
        }
        else // 3,2,1
            *p3 = tmp;
    }
}

static void fillRange(int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2, uint32_t pattern[32],uint32_t *bitmap)
{
    int32_t x1 = *x1p, x2 = *x2p;

    if (endy < 0)
    {
        int dy = endy - y;
        *x1p = x1 + dy * dx1;
        *x2p = x2 + dy * dx2;
        return;
    }

    if (y < 0)
    {
        x1 -= y * dx1;
        x2 -= y * dx2;
        y = 0;
    }

    while (y < endy)
    {
        drawFragment(bitmap + y * LCD_ROWSIZE32, (x1 >> 16), (x2 >> 16) + 1, pattern[y & 31]);

        x1 += dx1;
        x2 += dx2;
        ++y;
    }

    *x1p = x1;
    *x2p = x2;
}

static inline int32_t slope(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;

    if (dy < 1)
        return dx * (1 << 16);
    else
        return dx / dy * (1 << 16);
}

// from Playdate C examples
// created by Dave Hayden on 10/20/15.
void trifill(Point3du* verts, uint32_t* dither, uint32_t* bitmap)
{
    // sort by y coord
    Point3du* p1 = verts, * p2 = verts + 1, * p3 = verts + 2;
    sortTri(&p1,&p2,&p3);

    int endy = p3->y;
    if (endy >= LCD_ROWS) endy = LCD_ROWS;

    if (p1->y > LCD_ROWS || endy < 0) return ;

    int32_t x1 = p1->x * (1 << 16);
    int32_t x2 = x1;

    int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
    int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

    int32_t dx1 = sb;
    if (dx1 > sc) dx1 = sc;
    int32_t dx2 = sb;
    if (dx2 < sc) dx2 = sc;

    int midy = p2->y; 
    if (midy >= LCD_ROWS) midy = LCD_ROWS;
    fillRange(p1->y, midy, &x1, dx1, &x2, dx2, dither, bitmap);

    int dx = slope(p2->x, p2->y, p3->x, p3->y);

    if (sb < sc)
    {
        x1 = p2->x * (1 << 16);
        fillRange(p2->y, endy, &x1, dx, &x2, dx2, dither, bitmap);
    }
    else
    {
        x2 = p2->x * (1 << 16);
        fillRange(p2->y, endy, &x1, dx1, &x2, dx, dither, bitmap);
    }
}

void polyfill(const Point3du* verts, const int n, uint32_t* dither, uint32_t* bitmap) {
    BEGIN_FUNC();

	float miny = FLT_MAX, maxy = -FLT_MAX;
	int mini = -1;
	// find extent
	for (int i = 0; i < n; ++i) {
		float y = verts[i].y;
		if (y < miny) miny = y, mini = i;
		if (y > maxy) maxy = y;
	}
    // out of screen?
    if (miny > LCD_ROWS || maxy < 0) {
        END_FUNC();
        return;
    }
    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny <= 0.f) miny = -1.f;

	// data for left& right edges :
	int lj = mini, rj = mini;
    int ly = -1, ry = -1;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;

    for (int y = (int)miny+1; y < maxy; y++) {
        // maybe update to next vert
        while (ly < y) {
            const Point3du* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3du* p1 = &verts[lj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ly = (int)y1;
            lx = __TOFIXED16(p0->x);
            ldx = __TOFIXED16((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            lx += (int)(cy * ldx);
        }
        while (ry < y) {
            const Point3du* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3du* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ry = (int)y1;
            rx = __TOFIXED16(p0->x);
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
        }

        drawFragment(bitmap + y * LCD_ROWSIZE32, lx>>16, rx>>16, dither[y&31]);

        lx += ldx;
        rx += rdx;
    } 

    END_FUNC();
}

// affine texturing (using dither pattern)
// z contains dither color
void texfill(const Point3du* verts, const int n, uint8_t* dither_ramp, uint8_t* bitmap) {
    BEGIN_FUNC();

    float miny = FLT_MAX, maxy = -FLT_MAX;
    int mini = -1;
    // find extent
    for (int i = 0; i < n; ++i) {
        float y = verts[i].y;
        if (y < miny) miny = y, mini = i;
        if (y > maxy) maxy = y;
    }
    // out of screen?
    if (miny > LCD_ROWS || maxy < 0) {
        END_FUNC();
        return;
    }

    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny <= 0.f) miny = -1.f;

    // data for left& right edges :
    int lj = mini, rj = mini;
    int ly = -1, ry = -1;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;
    int lu = 0, ldu = 0, ru = 0, rdu = 0;
    for (int y = (int)miny+1; y < maxy; ++y) {
        // maybe update to next vert
        while (ly < y) {
            const Point3du* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3du* p1 = &verts[lj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ly = (int)y1;
            lx = __TOFIXED16(p0->x);
            lu = __TOFIXED16(p0->u);
            ldx = __TOFIXED16((p1->x - p0->x) / dy);
            ldu = __TOFIXED16((p1->u - p0->u) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            lx += (int)(cy * ldx);
            lu += (int)(cy * ldu);
        }
        while (ry < y) {
            const Point3du* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3du* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ry = (int)y1;
            rx = __TOFIXED16(p0->x);
            ru = __TOFIXED16(p0->u);
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            rdu = __TOFIXED16((p1->u - p0->u) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
            ru += (int)(cy * rdu);
        }
        drawTextureFragment(bitmap + y * LCD_ROWSIZE, lx >> 16, rx >> 16, lu, ru, dither_ramp + (y & 31) * 8 * 16);

        lx += ldx;
        rx += rdx;
        lu += ldu;
        ru += rdu;
    }
    END_FUNC();
}

// alpha polyfill
static void drawAlphaFragment(uint32_t* row, int x1, int x2, uint32_t color, uint32_t alpha)
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

        _drawMaskPattern(p, alpha & mask, color);
    }
    else
    {
        int x = x1;

        if (startbit > 0)
        {
            _drawMaskPattern(p++, alpha & startmask, color);
            x += (32 - startbit);
        }

        while (x + 32 <= x2)
        {
            _drawMaskPattern(p++, alpha, color);
            x += 32;
        }

        if (endbit > 0)
            _drawMaskPattern(p, alpha & endmask, color);
    }
}

void alphafill(const Point3du* verts, const int n, uint32_t color, uint32_t* alpha, uint32_t* bitmap) {
    BEGIN_FUNC();

    float miny = FLT_MAX, maxy = -FLT_MAX;
    int mini = -1;
    // find extent
    for (int i = 0; i < n; ++i) {
        float y = verts[i].y;
        if (y < miny) miny = y, mini = i;
        if (y > maxy) maxy = y;
    }
    // out of screen?
    if (miny > LCD_ROWS || maxy < 0) {
        END_FUNC();
        return;
    }

    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny <= 0.f) miny = -1.f;

    // data for left& right edges :
    int lj = mini, rj = mini;
    int ly = -1, ry = -1;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;
    
    for (int y = (int)miny; y < maxy; y++) {
        // maybe update to next vert
        while (ly < y) {
            const Point3du* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3du* p1 = &verts[lj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ly = (int)y1;
            lx = __TOFIXED16(p0->x);
            ldx = __TOFIXED16((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            lx += (int)(cy * ldx);
        }
        while (ry < y) {
            const Point3du* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3du* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ry = (int)y1;
            rx = __TOFIXED16(p0->x);
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
        }

        drawAlphaFragment(bitmap + y*LCD_ROWSIZE32, lx >> 16, rx >> 16, color, alpha[y & 31]);

        lx += ldx;
        rx += rdx;
    }

    END_FUNC();
}
