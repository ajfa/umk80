/* platform.h — minimal system layer for the frontend.
 *
 * Two interchangeable implementations:
 *   platform_win32.c  plain Win32 (user32 + gdi32). Primary target: no
 *                     external dependencies at all, the executable runs on a
 *                     clean Windows with no DLLs to copy.
 *   platform_sdl2.c   SDL2, for Linux and macOS.
 *
 * The emulator core sees none of this.
 */
#ifndef UMK80_PLATFORM_H
#define UMK80_PLATFORM_H

#include <stdint.h>

typedef enum {
    EV_NONE = 0,
    EV_QUIT,
    EV_KEYDOWN,
    EV_KEYUP,
    EV_MOUSEDOWN,
    EV_MOUSEUP
} ev_kind_t;

/* Normalised key codes: uppercase ASCII for printable keys, and above
 * 0x100 for everything else. */
#define PK_BACK   0x108
#define PK_ENTER  0x10D
#define PK_ESC    0x11B
#define PK_F1     0x201
#define PK_F12    0x20C

typedef struct {
    ev_kind_t kind;
    int key;        /* EV_KEY*  */
    int x, y;       /* EV_MOUSE*, in the panel's logical coordinates */
} plat_event_t;

/* Prepares the console so text comes out right BEFORE the window opens. On
 * Windows it sets the output code page to UTF-8, because cmd.exe starts in
 * 850 or 437 and mangles Cyrillic and accents. On POSIX it does nothing.
 * Called at the top of main. */
void plat_init(void);

/* Opens the window. `title` is UTF-8. `lw`/`lh` is the panel's logical size;
 * scaling to the real window is the platform's job, and plat_poll already
 * returns mouse coordinates converted into that logical space. */
int  plat_open(const char *title, int lw, int lh, int scale_num, int scale_den);
void plat_close(void);

/* Returns 1 and fills `ev` if there was an event; 0 if there are no more. */
int  plat_poll(plat_event_t *ev);

/* Blits the logical framebuffer to the window, scaling. */
void plat_present(const uint32_t *px);

void     plat_sleep_ms(unsigned ms);
uint64_t plat_ticks_ms(void);

#endif /* UMK80_PLATFORM_H */
