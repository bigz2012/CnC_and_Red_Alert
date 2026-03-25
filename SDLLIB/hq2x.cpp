/*
**  Optimized 2xBR pixel-art upscaling filter.
**  Multithreaded with fast bitwise color distance.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <SDL.h>

/*
**  Ultra-fast color distance using bitwise channel extraction.
**  Returns a weighted perceptual distance — zero means identical.
*/
static inline int fast_dist(uint32_t a, uint32_t b)
{
    if (a == b) return 0;
    /* Extract channels via shift+mask — no branches */
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    /* Absolute value via branchless trick */
    dr = (dr ^ (dr >> 31)) - (dr >> 31);
    dg = (dg ^ (dg >> 31)) - (dg >> 31);
    db = (db ^ (db >> 31)) - (db >> 31);
    /* Perceptual weights: G*4 + R*2 + B */
    return (dr << 1) + (dg << 2) + db;
}

/* Sharper blends — keep most of the center pixel */
static inline uint32_t blend_5_3(uint32_t a, uint32_t b)
{
    uint32_t rb = ((a & 0xFF00FF) * 7 + (b & 0xFF00FF)) >> 3 & 0xFF00FF;
    uint32_t g  = ((a & 0x00FF00) * 7 + (b & 0x00FF00)) >> 3 & 0x00FF00;
    return rb | g;
}

static inline uint32_t blend_7_1(uint32_t a, uint32_t b)
{
    uint32_t rb = ((a & 0xFF00FF) * 15 + (b & 0xFF00FF)) >> 4 & 0xFF00FF;
    uint32_t g  = ((a & 0x00FF00) * 15 + (b & 0x00FF00)) >> 4 & 0x00FF00;
    return rb | g;
}

static inline int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/*
**  Process a horizontal strip of the image (for multithreading).
*/
struct StripWork {
    const uint32_t *src;
    int src_w, src_h, src_pitch;
    uint32_t *dst;
    int dst_pitch;
    int y_start, y_end;
};

static int process_strip(void *data)
{
    StripWork *w = (StripWork *)data;
    const uint32_t *src = w->src;
    int src_w = w->src_w, src_h = w->src_h, src_pitch = w->src_pitch;
    uint32_t *dst = w->dst;
    int dst_pitch = w->dst_pitch;
    int last = src_w - 1;

    for (int y = w->y_start; y < w->y_end; y++)
    {
        int ym1 = y > 0 ? y - 1 : 0;
        int yp1 = y < src_h - 1 ? y + 1 : src_h - 1;
        int ym2 = y > 1 ? y - 2 : 0;
        int yp2 = y < src_h - 2 ? y + 2 : src_h - 1;

        const uint32_t *rm2 = (const uint32_t *)((const uint8_t *)src + ym2 * src_pitch);
        const uint32_t *rm1 = (const uint32_t *)((const uint8_t *)src + ym1 * src_pitch);
        const uint32_t *rc  = (const uint32_t *)((const uint8_t *)src + y * src_pitch);
        const uint32_t *rp1 = (const uint32_t *)((const uint8_t *)src + yp1 * src_pitch);
        const uint32_t *rp2 = (const uint32_t *)((const uint8_t *)src + yp2 * src_pitch);

        uint32_t *out0 = (uint32_t *)((uint8_t *)dst + (y * 2) * dst_pitch);
        uint32_t *out1 = (uint32_t *)((uint8_t *)dst + (y * 2 + 1) * dst_pitch);

        for (int x = 0; x < src_w; x++)
        {
            uint32_t PE = rc[x];

            int xm = x > 0 ? x - 1 : 0;
            int xp = x < last ? x + 1 : last;

            uint32_t b0 = rm1[xm], b1 = rm1[x], b2 = rm1[xp];
            uint32_t b3 = rc[xm],               b4 = rc[xp];
            uint32_t b5 = rp1[xm], b6 = rp1[x], b7 = rp1[xp];

            /*
            **  Fast flat-area rejection: check if all 8 neighbors match center
            **  using bitwise OR of XOR differences. If all zero, area is flat.
            */
            uint32_t diff = (PE ^ b0) | (PE ^ b1) | (PE ^ b2) | (PE ^ b3) |
                            (PE ^ b4) | (PE ^ b5) | (PE ^ b6) | (PE ^ b7);

            if (diff == 0) {
                /* All neighbors identical — write 4 copies and skip */
                out0[x*2]   = PE;
                out0[x*2+1] = PE;
                out1[x*2]   = PE;
                out1[x*2+1] = PE;
                continue;
            }

            /* Check if differences are minor (similar colors) */
            int max_d = fast_dist(PE, b0);
            int d;
            d = fast_dist(PE, b1); if (d > max_d) max_d = d;
            d = fast_dist(PE, b2); if (d > max_d) max_d = d;
            d = fast_dist(PE, b3); if (d > max_d) max_d = d;
            d = fast_dist(PE, b4); if (d > max_d) max_d = d;
            d = fast_dist(PE, b5); if (d > max_d) max_d = d;
            d = fast_dist(PE, b6); if (d > max_d) max_d = d;
            d = fast_dist(PE, b7); if (d > max_d) max_d = d;

            if (max_d < 24) {
                out0[x*2]   = PE;
                out0[x*2+1] = PE;
                out1[x*2]   = PE;
                out1[x*2+1] = PE;
                continue;
            }

            /* Extended neighbors for edge detection */
            int xm2 = x > 1 ? x - 2 : 0;
            int xp2 = x < last - 1 ? x + 2 : last;

            uint32_t a1 = rm2[x];
            uint32_t a3 = rc[xm2];
            uint32_t a4 = rc[xp2];
            uint32_t a6 = rp2[x];

            uint32_t e0 = PE, e1 = PE, e2 = PE, e3 = PE;

            /* Precompute shared distances */
            int d_b1_b3 = fast_dist(b1, b3);
            int d_b1_b4 = fast_dist(b1, b4);
            int d_b3_b6 = fast_dist(b3, b6);
            int d_b4_b6 = fast_dist(b4, b6);
            int d_PE_b0 = fast_dist(PE, b0);
            int d_PE_b2 = fast_dist(PE, b2);
            int d_PE_b5 = fast_dist(PE, b5);
            int d_PE_b7 = fast_dist(PE, b7);

            /* TOP-LEFT */
            {
                int dA = d_PE_b0 + fast_dist(b0, a1) + fast_dist(b0, a3) + d_b1_b4 + d_b3_b6;
                int dB = d_b1_b3 + fast_dist(PE, a1) + fast_dist(PE, a3) + d_PE_b2 + d_PE_b5;
                if (dA < dB)
                    e0 = (fast_dist(b3, b0) <= fast_dist(b1, b0)) ? blend_5_3(PE, b3) : blend_5_3(PE, b1);
                else if (dB < dA)
                    e0 = (fast_dist(PE, b1) <= fast_dist(PE, b3)) ? blend_7_1(PE, b1) : blend_7_1(PE, b3);
            }

            /* TOP-RIGHT */
            {
                int dA = d_PE_b2 + fast_dist(b2, a1) + fast_dist(b2, a4) + d_b1_b3 + d_b4_b6;
                int dB = d_b1_b4 + fast_dist(PE, a1) + fast_dist(PE, a4) + d_PE_b0 + d_PE_b7;
                if (dA < dB)
                    e1 = (fast_dist(b4, b2) <= fast_dist(b1, b2)) ? blend_5_3(PE, b4) : blend_5_3(PE, b1);
                else if (dB < dA)
                    e1 = (fast_dist(PE, b1) <= fast_dist(PE, b4)) ? blend_7_1(PE, b1) : blend_7_1(PE, b4);
            }

            /* BOTTOM-LEFT */
            {
                int dA = d_PE_b5 + fast_dist(b5, a3) + fast_dist(b5, a6) + d_b1_b3 + d_b4_b6;
                int dB = d_b3_b6 + fast_dist(PE, a3) + fast_dist(PE, a6) + d_PE_b0 + d_PE_b7;
                if (dA < dB)
                    e2 = (fast_dist(b6, b5) <= fast_dist(b3, b5)) ? blend_5_3(PE, b6) : blend_5_3(PE, b3);
                else if (dB < dA)
                    e2 = (fast_dist(PE, b3) <= fast_dist(PE, b6)) ? blend_7_1(PE, b3) : blend_7_1(PE, b6);
            }

            /* BOTTOM-RIGHT */
            {
                int dA = d_PE_b7 + fast_dist(b7, a4) + fast_dist(b7, a6) + d_b1_b4 + d_b3_b6;
                int dB = d_b4_b6 + fast_dist(PE, a4) + fast_dist(PE, a6) + d_PE_b2 + d_PE_b5;
                if (dA < dB)
                    e3 = (fast_dist(b6, b7) <= fast_dist(b4, b7)) ? blend_5_3(PE, b6) : blend_5_3(PE, b4);
                else if (dB < dA)
                    e3 = (fast_dist(PE, b4) <= fast_dist(PE, b6)) ? blend_7_1(PE, b4) : blend_7_1(PE, b6);
            }

            out0[x*2]   = e0;
            out0[x*2+1] = e1;
            out1[x*2]   = e2;
            out1[x*2+1] = e3;
        }
    }
    return 0;
}

/*
**  Number of worker threads for the upscaler.
*/
#define XBR_THREADS 4

static SDL_Thread *threads[XBR_THREADS];
static StripWork work[XBR_THREADS];

extern "C" void HQ2x_32(const uint32_t *src, int src_w, int src_h, int src_pitch,
                          uint32_t *dst, int dst_pitch)
{
    int rows_per_thread = src_h / XBR_THREADS;

    for (int i = 0; i < XBR_THREADS; i++) {
        work[i].src = src;
        work[i].src_w = src_w;
        work[i].src_h = src_h;
        work[i].src_pitch = src_pitch;
        work[i].dst = dst;
        work[i].dst_pitch = dst_pitch;
        work[i].y_start = i * rows_per_thread;
        work[i].y_end = (i == XBR_THREADS - 1) ? src_h : (i + 1) * rows_per_thread;

        threads[i] = SDL_CreateThread(process_strip, "xbr", &work[i]);
    }

    for (int i = 0; i < XBR_THREADS; i++) {
        SDL_WaitThread(threads[i], NULL);
    }
}
