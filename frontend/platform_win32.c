/* platform_win32.c — ventana y entrada con Win32 puro.
 *
 * Sólo user32 y gdi32, que van en el propio Windows: el .exe arranca en una
 * máquina limpia sin copiar ninguna DLL. Ver PLAN.md §2.
 */

#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HWND      g_hwnd;
static int       g_lw, g_lh;          /* tamaño lógico del panel */
static int       g_cw, g_ch;          /* tamaño del área de cliente */
static BITMAPINFO g_bmi;

#define EVQ_SIZE 256
static plat_event_t g_evq[EVQ_SIZE];
static int g_ev_head, g_ev_tail;

static void ev_push(ev_kind_t kind, int key, int x, int y)
{
    int next = (g_ev_head + 1) % EVQ_SIZE;
    if (next == g_ev_tail) return;    /* cola llena: se descarta */
    g_evq[g_ev_head].kind = kind;
    g_evq[g_ev_head].key  = key;
    g_evq[g_ev_head].x    = x;
    g_evq[g_ev_head].y    = y;
    g_ev_head = next;
}

static int map_vk(WPARAM vk)
{
    if (vk >= '0' && vk <= '9') return (int)vk;
    if (vk >= 'A' && vk <= 'Z') return (int)vk;
    if (vk >= VK_F1 && vk <= VK_F12) return PK_F1 + (int)(vk - VK_F1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return '0' + (int)(vk - VK_NUMPAD0);
    switch (vk) {
        case VK_SPACE:  return ' ';
        case VK_RETURN: return PK_ENTER;
        case VK_ESCAPE: return PK_ESC;
        case VK_BACK:   return PK_BACK;
        default:        return 0;
    }
}

/* Coordenadas de ventana -> coordenadas lógicas del panel. */
static void to_logical(int wx, int wy, int *lx, int *ly)
{
    *lx = (g_cw > 0) ? (wx * g_lw) / g_cw : 0;
    *ly = (g_ch > 0) ? (wy * g_lh) / g_ch : 0;
}

void plat_init(void)
{
    /* cmd.exe arranca en la página de códigos 850 (o la 437), en la que los
     * bytes UTF-8 salen como galimatías. Ver la nota del README. */
    SetConsoleOutputCP(CP_UTF8);
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            ev_push(EV_QUIT, 0, 0, 0);
            return 0;

        case WM_SIZE:
            g_cw = LOWORD(lp);
            g_ch = HIWORD(lp);
            return 0;

        case WM_ERASEBKGND:
            return 1;                    /* lo pinta plat_present */

        case WM_KEYDOWN:
            if (!(lp & (1 << 30))) {     /* ignorar autorrepetición */
                int k = map_vk(wp);
                if (k) ev_push(EV_KEYDOWN, k, 0, 0);
            }
            return 0;

        case WM_KEYUP: {
            int k = map_vk(wp);
            if (k) ev_push(EV_KEYUP, k, 0, 0);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int lx, ly;
            SetCapture(h);
            to_logical((int)(short)LOWORD(lp), (int)(short)HIWORD(lp), &lx, &ly);
            ev_push(EV_MOUSEDOWN, 0, lx, ly);
            return 0;
        }

        case WM_LBUTTONUP: {
            int lx, ly;
            ReleaseCapture();
            to_logical((int)(short)LOWORD(lp), (int)(short)HIWORD(lp), &lx, &ly);
            ev_push(EV_MOUSEUP, 0, lx, ly);
            return 0;
        }

        default:
            return DefWindowProcW(h, msg, wp, lp);
    }
}

int plat_open(const char *title, int lw, int lh, int scale_num, int scale_den)
{
    WNDCLASSW wc;
    RECT r;
    int ww, wh;
    wchar_t wtitle[256];

    g_lw = lw; g_lh = lh;
    ww = lw * scale_num / scale_den;
    wh = lh * scale_num / scale_den;
    g_cw = ww; g_ch = wh;

    /* El título lleva cirílico. Con la API -A, Windows interpretaría los
     * bytes UTF-8 en la página ANSI del sistema y saldría «Ð£ÐœÐš-80». Hay
     * que pasar por UTF-16 y usar la API ancha de punta a punta. */
    if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle,
                            (int)(sizeof wtitle / sizeof wtitle[0])) == 0) {
        wtitle[0] = L'U'; wtitle[1] = L'M'; wtitle[2] = L'K'; wtitle[3] = L'\0';
    }

    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = GetModuleHandleW(NULL);
    /* IDC_ARROW no es una cadena sino un átomo entero disfrazado de puntero;
     * con la API ancha hay que presentarlo como LPCWSTR. */
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"UMK80Panel";
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    if (!RegisterClassW(&wc)) return 0;

    r.left = 0; r.top = 0; r.right = ww; r.bottom = wh;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    g_hwnd = CreateWindowW(L"UMK80Panel", wtitle, WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top,
                           NULL, NULL, wc.hInstance, NULL);
    if (!g_hwnd) return 0;

    ZeroMemory(&g_bmi, sizeof g_bmi);
    g_bmi.bmiHeader.biSize        = sizeof g_bmi.bmiHeader;
    g_bmi.bmiHeader.biWidth       = lw;
    g_bmi.bmiHeader.biHeight      = -lh;      /* de arriba abajo */
    g_bmi.bmiHeader.biPlanes      = 1;
    g_bmi.bmiHeader.biBitCount    = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    return 1;
}

void plat_close(void)
{
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }
}

int plat_poll(plat_event_t *ev)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (g_ev_tail == g_ev_head) return 0;
    *ev = g_evq[g_ev_tail];
    g_ev_tail = (g_ev_tail + 1) % EVQ_SIZE;
    return 1;
}

void plat_present(const uint32_t *px)
{
    HDC dc = GetDC(g_hwnd);
    SetStretchBltMode(dc, HALFTONE);
    StretchDIBits(dc, 0, 0, g_cw, g_ch, 0, 0, g_lw, g_lh,
                  px, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(g_hwnd, dc);
}

void plat_sleep_ms(unsigned ms) { Sleep(ms); }

uint64_t plat_ticks_ms(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart * 1000) / freq.QuadPart);
}
