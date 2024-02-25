#include <pd_api.h>
#include <float.h>
#include "simd.h"
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

static void drawTextureFragment(uint32_t* row, int x1, int x2, int lu, int ru, uint32_t* dither_ramp)
{
    if (x2 < 0 || x1 >= LCD_COLUMNS)
        return;

    int dx = x2 - x1;
    if (dx == 0) return;
    // source is fixed point already
    uint32_t du = (ru - lu) / dx;
    if (x1 < 0) {
        lu -= x1 * du;
        x1 = 0;
    }

    if (x2 > LCD_COLUMNS)
        x2 = LCD_COLUMNS;

    if (x1 > x2)
        return;

    // get fractional delta u
    uint32_t fdu = du << 16;
    du >>= 16;
    uint32_t flu = lu << 16;
    dither_ramp += (lu >> 16);    

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

        // _drawMaskTexture(p, mask, dither_ramp);
        uint32_t pixels = 0;
        for (uint32_t dst_mask = 0x80000000 >> startbit; dst_mask != 0x80000000>>endbit; dst_mask>>=1) {
            pixels += (*dither_ramp) & dst_mask;
            __ADC(&flu, fdu, &dither_ramp, du);
        }
        *p = (*p & ~mask) | swap(pixels);
    }
    else
    {
        int x = x1;

        if (startbit > 0)
        {
            uint32_t pixels = 0;
            for (uint32_t dst_mask = 0x80000000 >> startbit; dst_mask != 0; dst_mask >>= 1) {
                pixels += (*dither_ramp) & dst_mask;
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            // _drawMaskTexture(p++, startmask, dither_ramp);
            x += (32 - startbit);
            *p = (*p & ~startmask) | swap(pixels);
            p++;
        }

        while (x + 32 <= x2)
        {
            // _drawMaskTextureOpaque(p++, color);
            uint32_t pixels = 0;
            for (uint32_t dst_mask = 0x80000000; dst_mask != 0; dst_mask >>= 1) {
                pixels += (*dither_ramp) & dst_mask;
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            x += 32;
            *(p++) = swap(pixels);
        }

        if (endbit > 0) {
            uint32_t pixels = 0;
            //_drawMaskTexture(p, endmask, color);
            for (uint32_t dst_mask = 0x80000000; dst_mask != 0x80000000 >> endbit; dst_mask >>= 1) {
                pixels += (*dither_ramp) & dst_mask;
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            *p = (*p & ~endmask) | swap(pixels);
        }
    }
}

// reversed u version
static void drawTextureFragmentRev(uint32_t* row, int x1, int x2, int ru, int lu, uint32_t* dither_ramp)
{
    if (x2 < 0 || x1 >= LCD_COLUMNS)
        return;

    int dx = x2 - x1;
    if (dx == 0) return;
    // source is fixed point already
    uint32_t du = (ru - lu) / dx;
    if (x1 < 0) {
        lu -= x1 * du;
        x1 = 0;
    }

    if (x2 > LCD_COLUMNS)
        x2 = LCD_COLUMNS;

    if (x1 > x2)
        return;

    // get fractional delta u
    uint32_t fdu = du << 16;
    du >>= 16;
    uint32_t flu = lu << 16;
    dither_ramp += (lu >> 16);

    // Operate on 32 bits at a time

    const int startbit = x1 & 31;
    const uint32_t startmask = swap((1 << (32 - startbit)) - 1);
    const int endbit = x2 & 31;
    const uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));

    int col = x2 / 32;
    uint32_t* p = row + col;

    if (col == x1 / 32)
    {
        uint32_t mask = 0;

        if (startbit > 0 && endbit > 0)
            mask = startmask & endmask;
        else if (startbit > 0)
            mask = startmask;
        else if (endbit > 0)
            mask = endmask;

        // _drawMaskTexture(p, mask, dither_ramp);
        uint32_t pixels = 0;
        for (int i = startbit; i < endbit; ++i) {
            pixels += (*dither_ramp) & (1<<(31-i));
            __ADC(&flu, fdu, &dither_ramp, du);
        }
        *p = (*p & ~mask) | swap(pixels);
    }
    else
    {
        int x = x2;
        if (endbit > 0) {
            uint32_t pixels = 0;
            //_drawMaskTexture(p, endmask, color);
            for (int i = endbit - 1; i >= 0; --i) {
                pixels += (*dither_ramp) & (0x80000000 >> i);
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            *p = (*p & ~endmask) | swap(pixels);
            x -= endbit;
        }
        p--;
        while (x - 32 >= x1)
        {
            // _drawMaskTextureOpaque(p++, color);
            uint32_t pixels = 0;
            for (int i = 31; i >= 0; --i) {
                pixels += (*dither_ramp) & (0x80000000 >> i);
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            x -= 32;
            *(p--) = swap(pixels);
        }

        if (startbit > 0)
        {
            uint32_t pixels = 0;
            for (int i = 31; i >= startbit; --i) {
                pixels += (*dither_ramp) & (0x80000000 >> i);
                __ADC(&flu, fdu, &dither_ramp, du);
            }
            // _drawMaskTexture(p++, startmask, dither_ramp);
            *p = (*p & ~startmask) | swap(pixels);
        }
    }
}

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
    if (miny > LCD_ROWS || maxy < 0) return;
    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny < 0) miny = 0;

	// data for left& right edges :
	int lj = mini, rj = mini;
    int ly = (int)miny, ry = (int)miny;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;
    bitmap = bitmap + (int)miny * LCD_ROWSIZE32;
    for (int y =(int)miny; y < maxy; ++y, bitmap+=LCD_ROWSIZE32) {
        // maybe update to next vert
        while (ly < y) {
            const Point3d* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3d* p1 = &verts[lj];
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
            const Point3d* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3d* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ry = (int)y1;
            rx = __TOFIXED16(p0->x);
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
        }

        drawFragment(bitmap, lx>>16, rx>>16, dither[y&31]);

        lx += ldx;
        rx += rdx;
    }    
}

// affine texturing (using dither pattern)
// z contains dither color
void texfill(const Point3d* verts, const int n, uint32_t* dither_ramp, uint32_t* bitmap) {
    float miny = FLT_MAX, maxy = FLT_MIN;
    int mini = -1;
    // find extent
    for (int i = 0; i < n; ++i) {
        float y = verts[i].y;
        if (y < miny) miny = y, mini = i;
        if (y > maxy) maxy = y;
    }
    // out of screen?
    if (miny > LCD_ROWS || maxy < 0) return;
    if (maxy > LCD_ROWS) maxy = LCD_ROWS;
    if (miny < 0) miny = 0;

    // data for left& right edges :
    int lj = mini, rj = mini;
    int ly = (int)miny, ry = (int)miny;
    int lx = 0, ldx = 0, rx = 0, rdx = 0;
    int lu = 0, ldu = 0, ru = 0, rdu = 0;
    bitmap = bitmap + (int)miny * LCD_ROWSIZE32;
    for (int y = (int)miny; y < maxy; ++y, bitmap += LCD_ROWSIZE32) {
        // maybe update to next vert
        while (ly < y) {
            const Point3d* p0 = &verts[lj];
            lj++;
            if (lj >= n) lj = 0;
            const Point3d* p1 = &verts[lj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ly = (int)y1;
            lx = __TOFIXED16(p0->x);
            lu = __TOFIXED16(p0->z);
            ldx = __TOFIXED16((p1->x - p0->x) / dy);
            ldu = __TOFIXED16((p1->z - p0->z) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            lx += (int)(cy * ldx);
            lu += (int)(cy * ldu);
        }
        while (ry < y) {
            const Point3d* p0 = &verts[rj];
            rj--;
            if (rj < 0) rj = n - 1;
            const Point3d* p1 = &verts[rj];
            const float y0 = p0->y, y1 = p1->y;
            const float dy = y1 - y0;
            ry = (int)y1;
            rx = __TOFIXED16(p0->x);
            ru = __TOFIXED16(p0->z);
            rdx = __TOFIXED16((p1->x - p0->x) / dy);
            rdu = __TOFIXED16((p1->z - p0->z) / dy);
            //sub - pixel correction
            const float cy = y - y0;
            rx += (int)(cy * rdx);
            ru += (int)(cy * rdu);
        }

        if (lu > ru) {
            // drawTextureFragmentRev(bitmap, lx >> 16, rx >> 16, lu, ru, dither_ramp + (y & 31) * 16);
        }
        else {
            drawTextureFragment(bitmap, lx >> 16, rx >> 16, lu, ru, dither_ramp + (y & 31) * 16);       
        }

        lx += ldx;
        rx += rdx;
        lu += ldu;
        ru += rdu;
    }
}


typedef struct {
    union {
        struct {
            uint8_t hilo;
            uint8_t hihi;
            uint8_t lolo;
            uint8_t lohi;
        };
        struct {
            uint16_t hi;
            uint16_t lo;
        };
        uint32_t v;
    };
} uint16_t_pair;

// draw a sprite a x/y with source scaled to w (source is sw * sw)
void sspr(int x, int y, int w, uint8_t* src, int sw, uint8_t* bitmap) {
    if (x >= LCD_COLUMNS || x + w < 0) return;
    if (y >= LCD_ROWS || y + w < 0) return;

    // fixed point 24:8
    const int FIXED_SHIFT = 8;
    int h = w;
    const uint16_t src_step_x = (sw << FIXED_SHIFT) / w;
    uint16_t_pair sdx = (uint16_t_pair){ .hi = 2 * src_step_x,.lo = 2 * src_step_x };
    const int sdy = (sw<<FIXED_SHIFT) / w;
    int sy = 0;

    if (x < 0) x = 0;
    if (x + w >= LCD_COLUMNS) w = LCD_COLUMNS - x - 1;
    if (y < 0) y = 0;
    if (y + w >= LCD_ROWS) h = LCD_ROWS - y - 1;


    // align to byte boundary
    const int x0 = 8 * (x / 8);
    const uint8_t left_mask = (0x000000ff << (8-(x&7)));
    // ceiling
    const int x1 = 8 * ((x + 8 - 1) / 8);
    const int x2 = 8 * ((x + w) / 8);
    const int x3 = x + w;
    const uint8_t right_mask = 0x0000ff00 >> (x3 & 7);
    
    // pd->system->logToConsole("x: %i x0: %i x1: %i x2: %i x3: %i l. mask: 0x%02x r. mask: 0x%02x", x, x0, x1, x2, x3, left_mask, right_mask);

    bitmap += x / 8 + y * LCD_ROWSIZE;    
    sw *= 2;
    /*
    1- pixel per loop
    cycles: 0.043588
    cycles : 0.042131
    cycles : 0.041841

    8 pixels per loop
    cycles: 0.010865
    cycles: 0.010859
    cycles: 0.011876

    2 pixels
    cycles: 0.013355
    cycles: 0.012962
    cycles: 0.012604

    cycles: 0.014759
    cycles: 0.012901
    cycles: 0.012849

    SADD16
    cycles: 0.015083
    cycles: 0.015953
    cycles: 0.015312

    unrolled + constant masks:
    cycles: 0.007522
    cycles: 0.007120
    cycles: 0.007267

    -----------------
    unrolled:
    cycles: 0.023058
    cycles: 0.021584
    cycles: 0.021456

    1 byte per pix: 40% cpu
    cycles: 0.014279
    cycles: 0.013815
    cycles: 0.013228

    1 byte per pix + alpha mask: 62% cpu
    cycles: 0.022014
    cycles: 0.022317
    cycles: 0.024208
    */

    for (int j = 0; j < h; ++j, bitmap += LCD_ROWSIZE, sy += sdy) {
        // offset source starting point by 
        uint8_t* dst = bitmap;
        uint8_t* src_row = src + (sy >> FIXED_SHIFT) * sw;
        // fill left column (if any) not aligned with 8 bit boundary
        uint16_t lsx = 0;
        if (left_mask) {
            uint8_t pixels = 0;
            uint8_t mask = left_mask;
            for (int i = x; i < x1; i++, lsx += src_step_x) {
                int src_x = lsx >> FIXED_SHIFT;
                int dst_shift = (0x80 >> (i & 7));
                pixels |= src_row[2 * src_x] * dst_shift;
                mask |= src_row[2 * src_x + 1] * dst_shift;
            }
            *dst = (*dst & ~mask) | pixels;
            dst++;
        }
        // fill bytes between 
        uint16_t_pair sx = (uint16_t_pair){ .hi = lsx, .lo = lsx + src_step_x };
        for (int i = x1; i < x2; i += 8, dst++) {
            // bench with smlad
            // hi * 0x80 + lo * 0x40
            uint8_t pixels =
                (src_row[2* sx.hihi] * 0x80) |
                (src_row[2* sx.lohi] * 0x40);
            uint8_t mask =
                (src_row[2 * sx.hihi + 1] * 0x80) |
                (src_row[2 * sx.lohi + 1] * 0x40);
            sx.v += sdx.v;
            pixels |=
                (src_row[2*sx.hihi] * 0x20) |
                (src_row[2*sx.lohi] * 0x10);
            mask |=
                (src_row[2 * sx.hihi + 1] * 0x20) |
                (src_row[2 * sx.lohi + 1] * 0x10);
            sx.v += sdx.v;
            pixels |=
                (src_row[2*sx.hihi] * 0x08) |
                (src_row[2*sx.lohi] * 0x04);
            mask |=
                (src_row[2 * sx.hihi + 1] * 0x08) |
                (src_row[2 * sx.lohi + 1] * 0x04);
            sx.v += sdx.v;
            pixels |=
                (src_row[2*sx.hihi] * 0x02) |
                (src_row[2*sx.lohi] * 0x01);
            mask |=
                (src_row[2 * sx.hihi + 1] * 0x02) |
                (src_row[2 * sx.lohi + 1] * 0x01);
            sx.v += sdx.v;

            *dst = (*dst & ~mask) | pixels;
        }
        if (right_mask) {
            uint16_t rsx = sx.lo;
            uint8_t pixels = 0;
            uint8_t mask = right_mask;
            for (int i = x2; i < x3; i++, rsx += src_step_x) {
                int src_x = rsx >> FIXED_SHIFT;
                int dst_shift = (0x80 >> (i & 7));
                pixels |= src_row[2 * src_x] * dst_shift;
                mask |= src_row[2 * src_x + 1] * dst_shift;
            }
            *dst = (*dst & ~ mask) | pixels;
        }
    }
}

static inline void bic(uint8_t* bitmap, int x, int len, uint8_t color) {
    color = !!color;
    const uint8_t mask = ((uint8_t)((int8_t)-1 >> len)) >> x;
    *bitmap = (*bitmap & ~mask) | (mask * color);
}

/*
cycles: 0.033123
cycles: 0.033349
cycles: 0.032799

*/
void upscale_image(int x, int y, int w, int h, uint8_t* src, int sw, int sh, uint8_t* bitmap) {
    const float ddx = (float)w / sw;
    const float ddy = (float)h / sh;
    float y1 = y;
    float y0 = y;
    // byte boundary
    bitmap += x / 8;
    
    for (int j = 0; j < sh; j++, y0 = y1, y1 += ddy, src += sw / 8) {
        uint8_t* dst = bitmap + (int)y0 * LCD_ROWSIZE;
        for (int k = (int)y0; k < (int)y1; k++, dst = bitmap + k * LCD_ROWSIZE) {
            float x1 = x & 7;
            float x0 = x1;
            for (int i = 0; i < sw; i++, x0 = x1, x1 += ddx) {
                uint8_t color = src[i / 8] & (0x80 >> (i & 7));
                int len = (int)x1 - (int)x0;
                int n = ((int)x0 + len) / 8;
                for (int kk = 0; kk < n; kk++) {
                    bic(dst, x0, 8 - (int)x0, color);
                    // todo: move out of loop?
                    len -= 8 - (int)x0;
                    // next 8 pixels block
                    dst++;
                    // reset boundaries
                    x0 = 0;
                    x1 = 0;
                }
                // draw remaining pixels
                bic(dst, x0, len, color);
            }
        }
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
        out |= (~((a->v[i] | b->v[i] | c->v[i]) >> 31)) & (0x80 >> i);
    }
    return out;
}

/*
cycles: 0.370769
cycles : 0.363643
cycles : 0.365406
*/
/*
static inline void vec_inc(Vec8i* a, Vec8i* b) {
    a->word[0] = __SADD16(a->word[0], b->word[0]);
    a->word[1] = __SADD16(a->word[1], b->word[1]);
    a->word[2] = __SADD16(a->word[2], b->word[2]);
    a->word[3] = __SADD16(a->word[3], b->word[3]);
}
*/

/*
cycles: 0.359701
cycles: 0.358622
cycles: 0.359048
*/
static inline Vec8i vec_inc(Vec8i a, Vec8i b) {
#ifdef  TARGET_PLAYDATE
    return (Vec8i) {
        .word = {
__SADD16(a.word[0], b.word[0]),
__SADD16(a.word[1], b.word[1]),
__SADD16(a.word[2], b.word[2]),
__SADD16(a.word[3], b.word[3]) }
    };
#else
    Vec8i out;
    for (int i = 0; i < 8; ++i) {
        out.v[i] = a.v[i] + b.v[i];
    }
    return out;
#endif //  TARGET_PLAYDATE
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
    bitmap += (minX >> 3) + minY * LCD_ROWSIZE;
    for (int y = minY; y <= maxY; y+=stepYSize, bitmap += LCD_ROWSIZE) {
        uint8_t* bitmap_row = bitmap;
        // Barycentric coordinates at start of row
        Vec8i w0 = w0_row;
        Vec8i w1 = w1_row;
        Vec8i w2 = w2_row;
        for (int x = minX; x <= maxX; x+=stepXSize, bitmap_row++) {
            // If p is on or inside all edges, render pixel.
            uint8_t mask = vec_mask(&w0, &w1, &w2);
            if (mask) {
                *bitmap_row = ((* bitmap_row) & ~mask) | mask;
            }
            // One step to the right            
            w0 = vec_inc(w0, e12.oneStepX);
            w1 = vec_inc(w1, e20.oneStepX);
            w2 = vec_inc(w2, e01.oneStepX);
        }
        // One row step        
        w0_row = vec_inc(w0_row, e12.oneStepY);
        w1_row = vec_inc(w1_row, e20.oneStepY);
        w2_row = vec_inc(w2_row, e01.oneStepY);
    }
}