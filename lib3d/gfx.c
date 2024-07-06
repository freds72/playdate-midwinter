#include <pd_api.h>
#include <float.h>
#include "simd.h"
#include "spall.h"
#include "gfx.h"

// float32 display ptr width
#define LCD_ROWSIZE32 (LCD_ROWSIZE/4)

static PlaydateAPI* pd = NULL;

void gfx_init(PlaydateAPI* playdate) {
    pd = playdate;
}


#if TARGET_PLAYDATE
static __attribute__((always_inline))
#else
static __forceinline
#endif
inline void _drawMaskPattern(uint32_t* p, uint32_t mask, uint32_t color)
{
    *p = (*p & ~mask) | (color & mask);
}

#if TARGET_PLAYDATE
static __attribute__((always_inline))
#else
static __forceinline
#endif
inline void _drawMaskPatternOpaque(uint32_t* p, uint32_t color)
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

    const int startbit = x1 & 31;
    const uint32_t startmask = swap((1 << (32 - startbit)) - 1);
    const int endbit = x2 & 31;
    const uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));

    const int col = x1 / 32;
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
            *p = (*p & ~startmask) | ((*(dither_ramp + (lu >> 16) * 8 + ((x>>3) & 3))) & startmask);
            x += (8 - startbit);
            lu += du;
            p++;
        }

        while (x + 8 <= x2)
        {
            *(p++) = *(dither_ramp + (lu >> 16) * 8 + ((x >>3) & 3));
            lu += du;
            x += 8;
        }

        if (endbit > 0) {
            // the last block of 8 pixels can overflow into negative territory
            if (lu < 0) lu = 0;           
            *p = (*p & ~endmask) | ((*(dither_ramp + (lu >> 16) * 8 + ((x>>3) & 3))) & endmask);
        }
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
            // lj = (lj + 1) % n;
            lj++;
            if (lj >= n) lj = 0;
            const Point3du* p1 = &verts[lj];
            const float y0 = p0->y, y1 = p1->y;
            ly = (int)y1;
            ldx = __TOFIXED16((p1->x - p0->x) / (y1 - y0));
            //sub - pixel correction
            lx = __TOFIXED16(p0->x) + (int)((y - y0) * ldx);
        }
        while (ry < y) {
            const Point3du* p0 = &verts[rj];
            // rj = (rj + n - 1) % n;
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3du* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            ry = (int)y1;
            rdx = __TOFIXED16((p1->x - p0->x) / (y1 - y0));
            //sub - pixel correction
            rx = __TOFIXED16(p0->x) + (int)((y - y0) * rdx);
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
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            rdu = __TOFIXED16((p1->u - p0->u) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx = __TOFIXED16(p0->x) + (int)(cy * rdx);
            ru = __TOFIXED16(p0->u) + (int)(cy * rdu);
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

    const int startbit = x1 & 31;
    const uint32_t startmask = swap((1 << (32 - startbit)) - 1);
    const int endbit = x2 & 31;
    const uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));

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
            ly = (int)y1;
            ldx = __TOFIXED16((p1->x - p0->x) / (y1 - y0));
            //sub - pixel correction
            lx = __TOFIXED16(p0->x) + (int)((y - y0) * ldx);
        }
        while (ry < y) {
            const Point3du* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3du* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            ry = (int)y1;
            rdx = __TOFIXED16((p1->x - p0->x) / (y1 - y0));
            //sub - pixel correction
            rx = __TOFIXED16(p0->x) + (int)((y - y0) * rdx);
        }

        drawAlphaFragment(bitmap + y * LCD_ROWSIZE32, lx >> 16, rx >> 16, color, alpha[y & 31]);

        lx += ldx;
        rx += rdx;
    }

    END_FUNC();
}
