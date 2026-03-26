#include <stdio.h>

#include <SDL.h>

#include "ww_win.h"
#include "net_select.h"
#include "gbuffer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

unsigned int WinX;
unsigned int WinY;
unsigned int Window;

SDL_Renderer *SDLRenderer;
Uint32 ForceRenderEventID;

/* Mouse wheel zoom state */
float ZoomLevel = 1.0f;
static const float ZOOM_MIN = 1.0f;
static const float ZOOM_MAX = 3.0f;
static const float ZOOM_STEP = 0.15f;
static int LogicalW = 0, LogicalH = 0;

int Change_Window(int windnum)
{
    printf("%s\n", __func__);
    return 0;
}

void SDL_Create_Main_Window(const char *title, int width, int height)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    // Create window at display resolution for borderless fullscreen feel
    // The game renders at width x height (which may be ultrawide e.g. 960x400)
    SDL_DisplayMode dm;
    int window_w = width;
    int window_h = height;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        // Scale to fit display height, maintaining the game's aspect ratio
        int scale = dm.h / height;
        if (scale < 1) scale = 1;
        window_w = width * scale;
        window_h = height * scale;
        // Clamp to display size
        if (window_w > dm.w) {
            window_w = dm.w;
            window_h = dm.h;
        }
    }

    MainWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w, window_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    ForceRenderEventID = SDL_RegisterEvents(1);

    SDLRenderer = SDL_CreateRenderer((SDL_Window *)MainWindow, -1, SDL_RENDERER_PRESENTVSYNC);

    /*
    **	Use bilinear filtering for smoother upscaling. This must be set BEFORE
    **	any textures are created as the hint applies at texture creation time.
    **	"linear" gives smooth scaling vs "nearest" which gives blocky pixels.
    */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    // Set logical size to game's internal resolution for correct mouse mapping.
    // The Scale2x upscaler renders to a 2x texture but we handle that in RenderCopy.
    SDL_RenderSetLogicalSize(SDLRenderer, width, height);
    LogicalW = width;
    LogicalH = height;

    // sometimes the window won't be created until it has content
    // so we get stuck waiting for focus, which it'll never get because it doesn't exist
    SDL_RenderClear(SDLRenderer);
    SDL_RenderPresent(SDLRenderer);
}

void SDL_Event_Loop()
{
#ifdef __EMSCRIPTEN__
    // sometimes we loop waiting for input
    // which isn't going to happen if the browser never gets control
    emscripten_sleep(0);
#endif

    // this is replacing WSAAsyncSelect, which would send through the windows event loop
    Socket_Select();

    SDL_Event event;
	while(SDL_PollEvent(&event))
    {
        if(event.type == ForceRenderEventID)
        {
            Video_End_Frame();
            continue;
        }
		SDL_Event_Handler(&event);
	}
}

void SDL_Send_Quit()
{
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
}