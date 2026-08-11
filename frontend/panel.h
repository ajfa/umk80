/* panel.h — dibujo del panel frontal del УМК-80 y prueba de impacto del ratón.
 *
 * El panel se dibuja vectorialmente. La geometría sale del Рис. 2 del ПС
 * (РР3.059.004 ПС, лист 14), que es el plano del propio fabricante, y la
 * paleta está medida sobre fotografías del equipo real (docs/ref/). No se
 * incrusta ninguna fotografía: el panel es todo trazado propio, escala a
 * cualquier resolución y no arrastra derechos de terceros.
 *
 * Todo el dibujo va a un framebuffer ARGB en memoria; no hay nada de
 * ventanas ni de sistema operativo aquí.
 */
#ifndef UMK80_PANEL_H
#define UMK80_PANEL_H

#include "umk80/umk80.h"

/* Resolución lógica del panel. El frontend la escala a la ventana. */
#define PANEL_W 1000
#define PANEL_H 640

typedef struct {
    uint32_t *px;
    int w, h;
} fb_t;

/* --- controles del panel --------------------------------------------------
 *
 * `col` y `row` son la posición en la matriz 6x4 del teclado, deducida de la
 * rutina CONV del monitor (ver DESCONOCIDOS.md §4). Los pulsadores y
 * conmutadores del borde derecho no están en la matriz: llevan col = -1.
 */
typedef enum {
    W_NONE = 0,
    W_KEY,          /* tecla de la matriz */
    W_BTN_SB,       /* СБ  — сброс, pulsador */
    W_BTN_PR,       /* ПР  — прерывание, pulsador */
    W_BTN_SHG,      /* ШГ  — шаг, pulsador */
    W_SW_RBSHG,     /* РБ/ШГ — conmutador CON enclavamiento */
    W_SW_KMCK       /* КМ/ЦК — conmutador CON enclavamiento */
} widget_kind_t;

typedef struct {
    widget_kind_t kind;
    int  col, row;          /* matriz del teclado, -1 si no aplica */
    int  x, y, w, h;
    const char *label;      /* rótulo principal */
    const char *sub;        /* segundo rótulo (PH, PL, SH, SL, H, L, A...) */
} widget_t;

extern const widget_t PANEL_WIDGETS[];
extern const int PANEL_WIDGET_COUNT;

/* Devuelve el índice del control bajo (x, y) en coordenadas lógicas del
 * panel, o -1. */
int panel_hit(int x, int y);

/* Dibuja el panel completo en `fb` (que debe medir PANEL_W x PANEL_H).
 * `held` es un mapa de qué controles se están dibujando pulsados: un byte
 * por control, indexado igual que PANEL_WIDGETS. */
void panel_draw(fb_t *fb, const umk_machine_t *m, const unsigned char *held);

#endif /* UMK80_PANEL_H */
