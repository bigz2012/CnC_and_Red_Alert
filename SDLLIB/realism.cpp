/*
**  Realism post-processing filter for pixel art.
**
**  Applied to the RGB frame BEFORE xBR upscaling for maximum performance.
**  Enhances the raw 960x400 frame with:
**    1. Local contrast enhancement (CLAHE-inspired)
**    2. Cinematic color grading (warm shift, saturation boost)
**    3. Edge ambient occlusion (depth cues at color boundaries)
**    4. Gradient smoothing (reduces 6-bit palette banding)
**
**  Multithreaded — splits work across strips like the xBR filter.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

/* Clamp integer to 0-255 */
static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

/*
**  Extract RGB channels from packed pixel.
*/
static inline void unpack(uint32_t c, int &r, int &g, int &b)
{
    r = (c >> 16) & 0xFF;
    g = (c >> 8) & 0xFF;
    b = c & 0xFF;
}

static inline uint32_t pack(int r, int g, int b)
{
    return ((uint32_t)clamp255(r) << 16) | ((uint32_t)clamp255(g) << 8) | (uint32_t)clamp255(b);
}

/*
**  Fast approximate luminance (0-255).
*/
static inline int luminance(int r, int g, int b)
{
    return (r * 77 + g * 150 + b * 29) >> 8;
}

struct RealismWork {
    uint32_t *pixels;
    int w, h, pitch;
    int y_start, y_end;
};

static int realism_strip(void *data)
{
    RealismWork *w = (RealismWork *)data;
    uint32_t *pixels = w->pixels;
    int width = w->w, height = w->h, pitch = w->pitch;

    for (int y = w->y_start; y < w->y_end; y++)
    {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);

        /* Get neighbor rows for edge detection */
        uint32_t *rowA = (y > 0) ? (uint32_t *)((uint8_t *)pixels + (y-1) * pitch) : row;
        uint32_t *rowB = (y < height-1) ? (uint32_t *)((uint8_t *)pixels + (y+1) * pitch) : row;

        for (int x = 0; x < width; x++)
        {
            int r, g, b;
            unpack(row[x], r, g, b);

            int lum = luminance(r, g, b);

            /* ============================================
            **  PASS 1: Local contrast enhancement
            **  Compare pixel luminance to neighborhood average.
            **  Boost pixels brighter than average, darken those below.
            ** ============================================ */
            int xm = x > 0 ? x - 1 : 0;
            int xp = x < width - 1 ? x + 1 : width - 1;

            int nr, ng, nb;
            int avg_lum = 0;

            /* Sample 4 cardinal neighbors for local average */
            unpack(rowA[x], nr, ng, nb);  avg_lum += luminance(nr, ng, nb);
            unpack(rowB[x], nr, ng, nb);  avg_lum += luminance(nr, ng, nb);
            unpack(row[xm], nr, ng, nb);  avg_lum += luminance(nr, ng, nb);
            unpack(row[xp], nr, ng, nb);  avg_lum += luminance(nr, ng, nb);
            avg_lum >>= 2;

            /* Subtle contrast boost: 12% enhancement */
            int contrast_delta = ((lum - avg_lum) * 12) >> 7;
            r += contrast_delta;
            g += contrast_delta;
            b += contrast_delta;

            /* ============================================
            **  PASS 2: Edge ambient occlusion
            **  Darken pixels near strong color boundaries
            **  to simulate shadow/depth at edges.
            ** ============================================ */
            uint32_t c = row[x];
            int edge_strength = 0;

            /* Check 4 neighbors for color difference */
            int d;
            d = (int)((c >> 16) & 0xFF) - (int)((rowA[x] >> 16) & 0xFF);
            d = (d ^ (d >> 31)) - (d >> 31);
            int dg1 = (int)((c >> 8) & 0xFF) - (int)((rowA[x] >> 8) & 0xFF);
            dg1 = (dg1 ^ (dg1 >> 31)) - (dg1 >> 31);
            edge_strength += d + dg1;

            d = (int)((c >> 16) & 0xFF) - (int)((rowB[x] >> 16) & 0xFF);
            d = (d ^ (d >> 31)) - (d >> 31);
            dg1 = (int)((c >> 8) & 0xFF) - (int)((rowB[x] >> 8) & 0xFF);
            dg1 = (dg1 ^ (dg1 >> 31)) - (dg1 >> 31);
            edge_strength += d + dg1;

            d = (int)((c >> 16) & 0xFF) - (int)((row[xm] >> 16) & 0xFF);
            d = (d ^ (d >> 31)) - (d >> 31);
            dg1 = (int)((c >> 8) & 0xFF) - (int)((row[xm] >> 8) & 0xFF);
            dg1 = (dg1 ^ (dg1 >> 31)) - (dg1 >> 31);
            edge_strength += d + dg1;

            d = (int)((c >> 16) & 0xFF) - (int)((row[xp] >> 16) & 0xFF);
            d = (d ^ (d >> 31)) - (d >> 31);
            dg1 = (int)((c >> 8) & 0xFF) - (int)((row[xp] >> 8) & 0xFF);
            dg1 = (dg1 ^ (dg1 >> 31)) - (dg1 >> 31);
            edge_strength += d + dg1;

            /* Apply subtle darkening at edges (ambient occlusion) */
            if (edge_strength > 80) {
                int ao = (edge_strength - 80) >> 3;
                if (ao > 12) ao = 12;
                r -= ao;
                g -= ao;
                b -= ao;
            }

            /* ============================================
            **  PASS 3: Cinematic color grading
            **  - Slight warm shift (boost reds/yellows in midtones)
            **  - Subtle saturation increase
            **  - Cool shadows, warm highlights
            ** ============================================ */

            /* Saturation boost: push channels away from grey */
            int grey = (r + g + b) / 3;
            r = grey + ((r - grey) * 140 >> 7);  /* ~10% saturation increase */
            g = grey + ((g - grey) * 140 >> 7);
            b = grey + ((b - grey) * 140 >> 7);

            /* Warm color shift: subtle red/yellow push in highlights */
            if (lum > 100) {
                int warmth = (lum - 100) >> 4;  /* 0-9 range */
                r += warmth;
                g += (warmth >> 1);  /* half as much green for warm yellow */
                /* b stays — less blue in highlights */
            }

            /* Cool shadows: slight blue push in darks */
            if (lum < 60) {
                int cool = (60 - lum) >> 4;
                b += cool;
                r -= (cool >> 1);
            }

            /* ============================================
            **  PASS 4: Gradient smoothing (de-banding)
            **  If neighboring pixels differ by exactly 4 (6-bit step),
            **  blend slightly to smooth the palette banding.
            ** ============================================ */
            {
                int lr, lg, lb, rr, rg, rb;
                unpack(row[xm], lr, lg, lb);
                unpack(row[xp], rr, rg, rb);

                /* Check for characteristic 6-bit banding (steps of 4) */
                int dr_l = r - lr; if (dr_l < 0) dr_l = -dr_l;
                int dr_r = r - rr; if (dr_r < 0) dr_r = -dr_r;
                if ((dr_l >= 3 && dr_l <= 5) && (dr_r >= 3 && dr_r <= 5)) {
                    r = (r * 3 + lr + rr) / 5;
                }

                int dg_l = g - lg; if (dg_l < 0) dg_l = -dg_l;
                int dg_r = g - rg; if (dg_r < 0) dg_r = -dg_r;
                if ((dg_l >= 3 && dg_l <= 5) && (dg_r >= 3 && dg_r <= 5)) {
                    g = (g * 3 + lg + rg) / 5;
                }

                int db_l = b - lb; if (db_l < 0) db_l = -db_l;
                int db_r = b - rb; if (db_r < 0) db_r = -db_r;
                if ((db_l >= 3 && db_l <= 5) && (db_r >= 3 && db_r <= 5)) {
                    b = (b * 3 + lb + rb) / 5;
                }
            }

            row[x] = pack(r, g, b);
        }
    }
    return 0;
}

#define REALISM_THREADS 4

extern "C" void Realism_Filter(uint32_t *pixels, int w, int h, int pitch)
{
    SDL_Thread *threads[REALISM_THREADS];
    RealismWork work[REALISM_THREADS];
    int rows_per = h / REALISM_THREADS;

    for (int i = 0; i < REALISM_THREADS; i++) {
        work[i].pixels = pixels;
        work[i].w = w;
        work[i].h = h;
        work[i].pitch = pitch;
        work[i].y_start = i * rows_per;
        work[i].y_end = (i == REALISM_THREADS - 1) ? h : (i + 1) * rows_per;
        threads[i] = SDL_CreateThread(realism_strip, "realism", &work[i]);
    }

    for (int i = 0; i < REALISM_THREADS; i++) {
        SDL_WaitThread(threads[i], NULL);
    }
}
