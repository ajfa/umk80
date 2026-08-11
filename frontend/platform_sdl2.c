/* platform_sdl2.c — ventana y entrada con SDL2, para Linux y macOS.
 *
 * En Windows no se usa: allí va platform_win32.c, que no necesita nada
 * instalado. Ver PLAN.md §2.
 */

#include "platform.h"

#include <SDL2/SDL.h>

static SDL_Window   *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture  *g_tex;
static int g_lw, g_lh;

static int map_sdl(SDL_Keycode k)
{
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (int)(k - SDLK_0);
    if (k >= SDLK_a && k <= SDLK_z) return 'A' + (int)(k - SDLK_a);
    if (k >= SDLK_F1 && k <= SDLK_F12) return PK_F1 + (int)(k - SDLK_F1);
    if (k >= SDLK_KP_1 && k <= SDLK_KP_9) return '1' + (int)(k - SDLK_KP_1);
    switch (k) {
        case SDLK_KP_0:      return '0';
        case SDLK_SPACE:     return ' ';
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  return PK_ENTER;
        case SDLK_ESCAPE:    return PK_ESC;
        case SDLK_BACKSPACE: return PK_BACK;
        default:             return 0;
    }
}

void plat_init(void)
{
    /* En POSIX la consola ya es UTF-8; no hay nada que preparar. */
}

int plat_open(const char *title, int lw, int lh, int scale_num, int scale_den)
{
    g_lw = lw; g_lh = lh;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 0;

    g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             lw * scale_num / scale_den, lh * scale_num / scale_den,
                             SDL_WINDOW_RESIZABLE);
    if (!g_win) return 0;

    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_SOFTWARE);
    if (!g_ren) return 0;

    /* Escalado con letterbox: el panel no se deforma al redimensionar. */
    SDL_RenderSetLogicalSize(g_ren, lw, lh);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, lw, lh);
    return g_tex != NULL;
}

void plat_close(void)
{
    if (g_tex) SDL_DestroyTexture(g_tex);
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    SDL_Quit();
}

int plat_poll(plat_event_t *ev)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                ev->kind = EV_QUIT;
                return 1;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                ev->key = map_sdl(e.key.keysym.sym);
                if (ev->key) { ev->kind = EV_KEYDOWN; return 1; }
                break;
            case SDL_KEYUP:
                ev->key = map_sdl(e.key.keysym.sym);
                if (ev->key) { ev->kind = EV_KEYUP; return 1; }
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                float lx = 0.0f, ly = 0.0f;
                if (e.button.button != SDL_BUTTON_LEFT) break;
                /* SDL_RenderSetLogicalSize ya ajusta el escalado; se
                 * convierte a coordenadas lógicas del panel. */
                SDL_RenderWindowToLogical(g_ren, e.button.x, e.button.y, &lx, &ly);
                ev->x = (int)lx;
                ev->y = (int)ly;
                ev->kind = (e.type == SDL_MOUSEBUTTONDOWN) ? EV_MOUSEDOWN : EV_MOUSEUP;
                return 1;
            }
            default:
                break;
        }
    }
    return 0;
}

void plat_present(const uint32_t *px)
{
    SDL_UpdateTexture(g_tex, NULL, px, g_lw * (int)sizeof(uint32_t));
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}

void plat_sleep_ms(unsigned ms) { SDL_Delay(ms); }

uint64_t plat_ticks_ms(void) { return (uint64_t)SDL_GetTicks(); }
