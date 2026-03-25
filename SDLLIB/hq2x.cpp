/*
**  Optimized 2xBR pixel-art upscaling filter.
**  Fast implementation with precomputed row pointers and
**  simplified color distance using bit tricks.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/*
**  Fast approximate color distance using weighted RGB.
**  Avoids YUV conversion — uses perceptual RGB weights instead.
**  Much faster than full YUV while giving similar results.
*/
static inline int fast_dist(uint32_t a, uint32_t b)
{
    if (a == b) return 0;
    int dr = (int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF);
    int dg = (int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF);
    int db = (int)(a & 0xFF) - (int)(b & 0xFF);
    /* Approximate perceptual weights: R*2 + G*4 + B*1 (shift-based) */
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    return (dr << 1) + (dg << 2) + db;
}

/* Sharper blends — less aggressive than before to preserve detail */
static inline uint32_t blend_5_3(uint32_t a, uint32_t b)
{
    /* Use 7:1 instead of 5:3 — keeps much more of the center pixel */
    uint32_t rb = ((a & 0xFF00FF) * 7 + (b & 0xFF00FF)) >> 3 & 0xFF00FF;
    uint32_t g  = ((a & 0x00FF00) * 7 + (b & 0x00FF00)) >> 3 & 0x00FF00;
    return rb | g;
}

static inline uint32_t blend_7_1(uint32_t a, uint32_t b)
{
    /* Very subtle — 15:1 blend for secondary edges */
    uint32_t rb = ((a & 0xFF00FF) * 15 + (b & 0xFF00FF)) >> 4 & 0xFF00FF;
    uint32_t g  = ((a & 0x00FF00) * 15 + (b & 0x00FF00)) >> 4 & 0x00FF00;
    return rb | g;
}

static inline uint32_t blend_3_1(uint32_t a, uint32_t b)
{
    uint32_t rb = ((a & 0xFF00FF) * 3 + (b & 0xFF00FF)) >> 2 & 0xFF00FF;
    uint32_t g  = ((a & 0x00FF00) * 3 + (b & 0x00FF00)) >> 2 & 0x00FF00;
    return rb | g;
}

/*
**  Clamp helper for row access.
*/
static inline int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

extern "C" void HQ2x_32(const uint32_t *src, int src_w, int src_h, int src_pitch,
                          uint32_t *dst, int dst_pitch)
{
    /* Precompute row pointers for all rows + 2-pixel border */
    const uint32_t **rows = (const uint32_t **)alloca(sizeof(const uint32_t *) * (src_h + 4));
    for (int y = -2; y < src_h + 2; y++) {
        int cy = clamp(y, 0, src_h - 1);
        rows[y + 2] = (const uint32_t *)((const uint8_t *)src + cy * src_pitch);
    }

    for (int y = 0; y < src_h; y++)
    {
        const uint32_t *rm2 = rows[y];       /* y-2 */
        const uint32_t *rm1 = rows[y + 1];   /* y-1 */
        const uint32_t *rc  = rows[y + 2];   /* y   */
        const uint32_t *rp1 = rows[y + 3];   /* y+1 */
        const uint32_t *rp2 = rows[y + 4];   /* y+2 */

        uint32_t *out0 = (uint32_t *)((uint8_t *)dst + (y * 2) * dst_pitch);
        uint32_t *out1 = (uint32_t *)((uint8_t *)dst + (y * 2 + 1) * dst_pitch);

        int last = src_w - 1;

        for (int x = 0; x < src_w; x++)
        {
            uint32_t PE = rc[x];

            /* 3x3 immediate neighbors */
            int xm = x > 0 ? x - 1 : 0;
            int xp = x < last ? x + 1 : last;

            uint32_t b0 = rm1[xm], b1 = rm1[x], b2 = rm1[xp];
            uint32_t b3 = rc[xm],               b4 = rc[xp];
            uint32_t b5 = rp1[xm], b6 = rp1[x], b7 = rp1[xp];

            /* Extended neighbors for xBR edge detection */
            int xm2 = x > 1 ? x - 2 : 0;
            int xp2 = x < last - 1 ? x + 2 : last;

            uint32_t a1 = rm2[x];
            uint32_t a3 = rc[xm2];
            uint32_t a4 = rc[xp2];
            uint32_t a6 = rp2[x];

            /* Default output */
            uint32_t e0 = PE, e1 = PE, e2 = PE, e3 = PE;

            /*
            **  Quick reject: if all immediate neighbors are very similar to center,
            **  this is a flat area — skip all edge detection and keep sharp.
            */
            int max_neighbor_dist = fast_dist(PE, b0);
            int d;
            d = fast_dist(PE, b1); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b2); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b3); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b4); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b5); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b6); if (d > max_neighbor_dist) max_neighbor_dist = d;
            d = fast_dist(PE, b7); if (d > max_neighbor_dist) max_neighbor_dist = d;

            if (max_neighbor_dist < 20) {
                /* Flat area — no blending needed, keep pixel-sharp */
                out0[x*2]   = PE;
                out0[x*2+1] = PE;
                out1[x*2]   = PE;
                out1[x*2+1] = PE;
                continue;
            }

            /* Precompute common distances */
            int d_b1_b3 = fast_dist(b1, b3);
            int d_b1_b4 = fast_dist(b1, b4);
            int d_b3_b6 = fast_dist(b3, b6);
            int d_b4_b6 = fast_dist(b4, b6);
            int d_PE_b0 = fast_dist(PE, b0);
            int d_PE_b2 = fast_dist(PE, b2);
            int d_PE_b5 = fast_dist(PE, b5);
            int d_PE_b7 = fast_dist(PE, b7);

            /*
            **  TOP-LEFT (e0): compare NW-SE diagonal vs NE-SW diagonal
            */
            {
                int dNWSE = d_PE_b0 + fast_dist(b0, a1) + fast_dist(b0, a3) + d_b1_b4 + d_b3_b6;
                int dNESW = d_b1_b3 + fast_dist(PE, a1) + fast_dist(PE, a3) + d_PE_b2 + d_PE_b5;

                if (dNWSE < dNESW) {
                    e0 = (fast_dist(b3, b0) <= fast_dist(b1, b0)) ? blend_5_3(PE, b3) : blend_5_3(PE, b1);
                } else if (dNESW < dNWSE) {
                    e0 = (fast_dist(PE, b1) <= fast_dist(PE, b3)) ? blend_7_1(PE, b1) : blend_7_1(PE, b3);
                }
            }

            /*
            **  TOP-RIGHT (e1)
            */
            {
                int dNESW = d_PE_b2 + fast_dist(b2, a1) + fast_dist(b2, a4) + d_b1_b3 + d_b4_b6;
                int dNWSE = d_b1_b4 + fast_dist(PE, a1) + fast_dist(PE, a4) + d_PE_b0 + d_PE_b7;

                if (dNESW < dNWSE) {
                    e1 = (fast_dist(b4, b2) <= fast_dist(b1, b2)) ? blend_5_3(PE, b4) : blend_5_3(PE, b1);
                } else if (dNWSE < dNESW) {
                    e1 = (fast_dist(PE, b1) <= fast_dist(PE, b4)) ? blend_7_1(PE, b1) : blend_7_1(PE, b4);
                }
            }

            /*
            **  BOTTOM-LEFT (e2)
            */
            {
                int dNESW = d_PE_b5 + fast_dist(b5, a3) + fast_dist(b5, a6) + d_b1_b3 + d_b4_b6;
                int dNWSE = d_b3_b6 + fast_dist(PE, a3) + fast_dist(PE, a6) + d_PE_b0 + d_PE_b7;

                if (dNESW < dNWSE) {
                    e2 = (fast_dist(b6, b5) <= fast_dist(b3, b5)) ? blend_5_3(PE, b6) : blend_5_3(PE, b3);
                } else if (dNWSE < dNESW) {
                    e2 = (fast_dist(PE, b3) <= fast_dist(PE, b6)) ? blend_7_1(PE, b3) : blend_7_1(PE, b6);
                }
            }

            /*
            **  BOTTOM-RIGHT (e3)
            */
            {
                int dNWSE = d_PE_b7 + fast_dist(b7, a4) + fast_dist(b7, a6) + d_b1_b4 + d_b3_b6;
                int dNESW = d_b4_b6 + fast_dist(PE, a4) + fast_dist(PE, a6) + d_PE_b2 + d_PE_b5;

                if (dNWSE < dNESW) {
                    e3 = (fast_dist(b6, b7) <= fast_dist(b4, b7)) ? blend_5_3(PE, b6) : blend_5_3(PE, b4);
                } else if (dNESW < dNWSE) {
                    e3 = (fast_dist(PE, b4) <= fast_dist(PE, b6)) ? blend_7_1(PE, b4) : blend_7_1(PE, b6);
                }
            }

            out0[x*2]   = e0;
            out0[x*2+1] = e1;
            out1[x*2]   = e2;
            out1[x*2+1] = e3;
        }
    }
}
