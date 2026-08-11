/* platform.h — capa mínima de sistema para el frontend.
 *
 * Dos realizaciones intercambiables:
 *   platform_win32.c  Win32 puro (user32 + gdi32). Objetivo primario: no
 *                     arrastra ninguna dependencia externa, el ejecutable
 *                     arranca en un Windows limpio sin copiar DLLs.
 *   platform_sdl2.c   SDL2, para Linux y macOS.
 *
 * El núcleo del emulador no ve nada de esto.
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

/* Códigos de tecla normalizados: ASCII en mayúsculas para lo imprimible,
 * y por encima de 0x100 lo demás. */
#define PK_BACK   0x108
#define PK_ENTER  0x10D
#define PK_ESC    0x11B
#define PK_F1     0x201
#define PK_F12    0x20C

typedef struct {
    ev_kind_t kind;
    int key;        /* EV_KEY*  */
    int x, y;       /* EV_MOUSE*, en coordenadas lógicas del panel */
} plat_event_t;

/* Abre la ventana. `lw`/`lh` es el tamaño lógico del panel; el escalado a la
 * ventana real lo hace la plataforma, y plat_poll ya devuelve las
 * coordenadas del ratón convertidas a ese espacio lógico. */
int  plat_open(const char *title, int lw, int lh, int scale_num, int scale_den);
void plat_close(void);

/* Devuelve 1 y rellena `ev` si había un evento; 0 si no hay más. */
int  plat_poll(plat_event_t *ev);

/* Vuelca el framebuffer lógico a la ventana, escalando. */
void plat_present(const uint32_t *px);

void     plat_sleep_ms(unsigned ms);
uint64_t plat_ticks_ms(void);

#endif /* UMK80_PLATFORM_H */
