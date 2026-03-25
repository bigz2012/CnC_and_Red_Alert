#include "gbuffer.h"

#include <SDL.h>
#include <string.h>
#include <stdlib.h>

/* HQ2x upscaler — implemented in hq2x.cpp */
extern "C" void HQ2x_32(const uint32_t *src, int src_w, int src_h, int src_pitch,
                          uint32_t *dst, int dst_pitch);

extern SDL_Renderer *SDLRenderer;

extern Uint32 ForceRenderEventID;
static Uint32 Force_Redraw_Timer(Uint32 interval, void *)
{
    SDL_Event ev;
    ev.type = ForceRenderEventID;
    SDL_PushEvent(&ev);
    return 0;
}

/*
**  Blend two RGB888 pixels by averaging their components.
*/
static inline uint32_t BlendPixels(uint32_t a, uint32_t b)
{
    uint32_t rb = (((a & 0xFF00FF) + (b & 0xFF00FF)) >> 1) & 0xFF00FF;
    uint32_t g  = (((a & 0x00FF00) + (b & 0x00FF00)) >> 1) & 0x00FF00;
    return rb | g;
}

/*
**  Blend four RGB888 pixels by averaging.
*/
static inline uint32_t BlendPixels4(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t rb = (((a & 0xFF00FF) + (b & 0xFF00FF) + (c & 0xFF00FF) + (d & 0xFF00FF)) >> 2) & 0xFF00FF;
    uint32_t g  = (((a & 0x00FF00) + (b & 0x00FF00) + (c & 0x00FF00) + (d & 0x00FF00)) >> 2) & 0x00FF00;
    return rb | g;
}

/*
**  Enhanced Scale2x with diagonal awareness.
**  Standard Scale2x for edges, plus diagonal interpolation for smoother curves.
*/
static void Scale2x_Enhanced(const uint32_t *src, int src_w, int src_h, int src_pitch,
                              uint32_t *dst, int dst_pitch)
{
    for (int y = 0; y < src_h; y++)
    {
        const uint32_t *row  = (const uint32_t *)((const uint8_t *)src + y * src_pitch);
        const uint32_t *rowA = (y > 0) ? (const uint32_t *)((const uint8_t *)src + (y-1) * src_pitch) : row;
        const uint32_t *rowD = (y < src_h-1) ? (const uint32_t *)((const uint8_t *)src + (y+1) * src_pitch) : row;

        uint32_t *out0 = (uint32_t *)((uint8_t *)dst + (y*2) * dst_pitch);
        uint32_t *out1 = (uint32_t *)((uint8_t *)dst + (y*2+1) * dst_pitch);

        for (int x = 0; x < src_w; x++)
        {
            uint32_t P = row[x];
            uint32_t A = rowA[x];
            uint32_t B = (x < src_w-1) ? row[x+1] : P;
            uint32_t C = (x > 0) ? row[x-1] : P;
            uint32_t D = rowD[x];

            /* Diagonals for enhanced detection */
            uint32_t E = (y > 0 && x > 0) ? rowA[x-1] : P;         /* top-left */
            uint32_t F = (y > 0 && x < src_w-1) ? rowA[x+1] : P;   /* top-right */
            uint32_t G = (y < src_h-1 && x > 0) ? rowD[x-1] : P;   /* bottom-left */
            uint32_t H = (y < src_h-1 && x < src_w-1) ? rowD[x+1] : P; /* bottom-right */

            uint32_t p0, p1, p2, p3;

            /* Standard Scale2x */
            p0 = (C == A && C != D && A != B) ? A : P;
            p1 = (A == B && A != C && B != D) ? B : P;
            p2 = (D == C && D != B && C != A) ? C : P;
            p3 = (B == D && B != A && D != C) ? D : P;

            /*
            **  Enhanced: smooth diagonal staircase patterns.
            **  When a diagonal neighbor matches and the adjacent cardinals don't,
            **  blend rather than hard-assign to reduce aliasing.
            */
            if (p0 == P && E == A && E == C && A != B && C != D) {
                p0 = BlendPixels(P, E);
            }
            if (p1 == P && F == A && F == B && A != C && B != D) {
                p1 = BlendPixels(P, F);
            }
            if (p2 == P && G == C && G == D && C != A && D != B) {
                p2 = BlendPixels(P, G);
            }
            if (p3 == P && H == B && H == D && B != A && D != C) {
                p3 = BlendPixels(P, H);
            }

            out0[x*2]   = p0;
            out0[x*2+1] = p1;
            out1[x*2]   = p2;
            out1[x*2+1] = p3;
        }
    }
}

/*
**  Scale4x: apply enhanced Scale2x twice for 4x resolution.
**  Uses a temporary buffer for the intermediate 2x pass.
*/
static uint32_t *scale4x_tmp = NULL;
static int scale4x_tmp_size = 0;

static void Scale4x_32(const uint32_t *src, int src_w, int src_h, int src_pitch,
                        uint32_t *dst, int dst_pitch)
{
    int mid_w = src_w * 2;
    int mid_h = src_h * 2;
    int mid_pitch = mid_w * sizeof(uint32_t);
    int needed = mid_w * mid_h;

    if (scale4x_tmp_size < needed) {
        free(scale4x_tmp);
        scale4x_tmp = (uint32_t *)malloc(needed * sizeof(uint32_t));
        scale4x_tmp_size = needed;
    }

    /* First pass: src (1x) -> tmp (2x) */
    Scale2x_Enhanced(src, src_w, src_h, src_pitch, scale4x_tmp, mid_pitch);

    /* Second pass: tmp (2x) -> dst (4x) */
    Scale2x_Enhanced(scale4x_tmp, mid_w, mid_h, mid_pitch, dst, dst_pitch);
}


bool GraphicBufferClass::Lock(void)
{
    if(!PaletteSurface)
        return true;

    if(!LockCount)
    {
        SDL_LockSurface((SDL_Surface *)PaletteSurface);
        Offset = (uint8_t *)((SDL_Surface *)PaletteSurface)->pixels;
    }

    LockCount++;
    return true;
}

bool GraphicBufferClass::Unlock(void)
{
    if(!PaletteSurface || !LockCount)
        return true;

    LockCount--;

    if(!LockCount)
    {
        SDL_UnlockSurface((SDL_Surface *)PaletteSurface);
        Offset = NULL;
        Update_Window_Surface(false);
    }

    return true;
}

void GraphicBufferClass::Update_Window_Surface(bool end_frame)
{
    auto window_tex = (SDL_Texture *)WindowTexture;
    auto upscale_tex = (SDL_Texture *)UpscaleTexture;

    if(!end_frame)
    {
        if(!RedrawTimer)
            RedrawTimer = SDL_AddTimer(1000/30, Force_Redraw_Timer, NULL);
        return;
    }

    if(RedrawTimer)
    {
        SDL_RemoveTimer(RedrawTimer);
        RedrawTimer = 0;
    }

    // blit from paletted surface to 1x RGB texture
    SDL_Surface *tmp_surf;
    SDL_LockTextureToSurface(window_tex, NULL, &tmp_surf);
    SDL_BlitSurface((SDL_Surface *)PaletteSurface, NULL, tmp_surf, NULL);

    if (upscale_tex)
    {
        // Apply HQ2x upscaling: 1x RGB -> 2x RGB with anti-aliased edges
        SDL_Surface *dst_surf;
        SDL_LockTextureToSurface(upscale_tex, NULL, &dst_surf);

        HQ2x_32(
            (const uint32_t *)tmp_surf->pixels, tmp_surf->w, tmp_surf->h, tmp_surf->pitch,
            (uint32_t *)dst_surf->pixels, dst_surf->pitch
        );

        SDL_UnlockTexture(window_tex);
        SDL_UnlockTexture(upscale_tex);

        // present the upscaled texture
        SDL_SetRenderDrawColor(SDLRenderer, 0, 0, 0, 255);
        SDL_RenderClear(SDLRenderer);
        SDL_RenderCopy(SDLRenderer, upscale_tex, NULL, NULL);
    }
    else
    {
        SDL_UnlockTexture(window_tex);

        SDL_SetRenderDrawColor(SDLRenderer, 0, 0, 0, 255);
        SDL_RenderClear(SDLRenderer);
        SDL_RenderCopy(SDLRenderer, window_tex, NULL, NULL);
    }

    SDL_RenderPresent(SDLRenderer);

    // update the event loop here too for now
    SDL_Event_Loop();
}

void GraphicBufferClass::Update_Palette(uint8_t *palette)
{
    auto sdl_pal = ((SDL_Surface *)PaletteSurface)->format->palette;

    bool changed = false;

    for(int i = 0; i < sdl_pal->ncolors; i++)
    {
        // convert from 6-bit
        int new_r = palette[i * 3 + 0] << 2 | palette[i * 3 + 0] >> 4;
        int new_g = palette[i * 3 + 1] << 2 | palette[i * 3 + 1] >> 4;
        int new_b = palette[i * 3 + 2] << 2 | palette[i * 3 + 2] >> 4;

        changed = changed || sdl_pal->colors[i].r != new_r || sdl_pal->colors[i].g != new_g || sdl_pal->colors[i].b != new_b;

        sdl_pal->colors[i].r = new_r;
        sdl_pal->colors[i].g = new_g;
        sdl_pal->colors[i].b = new_b;
    }

    if(!changed)
        return;

    SDL_SetPaletteColors(sdl_pal, sdl_pal->colors, 0, sdl_pal->ncolors);

    Update_Window_Surface(false);
}

const void *GraphicBufferClass::Get_Palette() const
{
    return ((SDL_Surface *)PaletteSurface)->format->palette;
}

void GraphicBufferClass::Init_Display_Surface()
{
    WindowTexture = SDL_CreateTexture(SDLRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, Width, Height);
    PaletteSurface = SDL_CreateRGBSurface(0, Width, Height, 8, 0, 0, 0, 0);

    // Create 2x upscaled texture for enhanced Scale2x output (e.g. 960x400 -> 1920x800)
    UpscaleTexture = SDL_CreateTexture(SDLRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, Width * 2, Height * 2);
}
