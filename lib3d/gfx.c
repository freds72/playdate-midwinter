#include <pd_api.h>
#include <float.h>
#include "gfx.h"

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

static inline uint32_t __SADD16(uint32_t op1, uint32_t op2)
{
#if TARGET_PLAYDATE
    uint32_t result;

    __asm volatile ("sadd16 %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
    return(result);
#else
    return op1 + op2;
#endif
}

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

void polyfill(Point3d* verts, int n, uint32_t* dither, uint32_t* bitmap) {
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
    float lx = 0, ldx = 0, rx = 0, rdx = 0;
    for (uint8_t y =((int)miny); y < maxy; ++y){
        // maybe update to next vert
        while (ly < y) {
            Point3d p0 = verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            Point3d p1 = verts[lj];
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
            Point3d p0 = verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            Point3d p1 = verts[rj];
            float y0 = p0.y, y1 = p1.y;
            float dy = y1 - y0;
            ry = (int)y1;
            rx = p0.x;
            rdx = (p1.x - rx) / dy;
            //sub - pixel correction
            float cy = y - y0;
            rx += cy * rdx;
        }

        drawFragment(bitmap + y * LCD_ROWSIZE32, (int)lx, (int)rx, dither[y%8]);

        lx += ldx;
        rx += rdx;
    }    
}
