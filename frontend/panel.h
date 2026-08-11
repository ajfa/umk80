/* panel.h — drawing of the УМК-80 front panel, and mouse hit testing.
 *
 * The panel is drawn vectorially. The geometry comes from Рис. 2 of the ПС
 * (РР3.059.004 ПС, лист 14), which is the manufacturer's own drawing, and the
 * palette is measured from photographs of the real machine (docs/ref/). No
 * photograph is embedded: the panel is entirely original linework, it scales
 * to any resolution and it carries no third-party rights.
 *
 * All drawing goes into an in-memory ARGB framebuffer; there is nothing about
 * windows or operating systems here.
 */
#ifndef UMK80_PANEL_H
#define UMK80_PANEL_H

#include "umk80/umk80.h"

/* Logical resolution of the panel. The frontend scales it to the window. */
#define PANEL_W 1000
#define PANEL_H 640

typedef struct {
    uint32_t *px;
    int w, h;
} fb_t;

/* --- panel controls -------------------------------------------------------
 *
 * `col` and `row` are the position in the 6x4 keyboard matrix, derived from
 * the monitor's CONV routine (see UNKNOWNS.md §4). The buttons and switches
 * along the right edge are not in the matrix: they carry col = -1.
 */
typedef enum {
    W_NONE = 0,
    W_KEY,          /* matrix key */
    W_BTN_SB,       /* СБ  — сброс, momentary */
    W_BTN_PR,       /* ПР  — прерывание, momentary */
    W_BTN_SHG,      /* ШГ  — шаг, momentary */
    W_SW_RBSHG,     /* РБ/ШГ — LATCHING switch */
    W_SW_KMCK       /* КМ/ЦК — LATCHING switch */
} widget_kind_t;

typedef struct {
    widget_kind_t kind;
    int  col, row;          /* keyboard matrix, -1 if not applicable */
    int  x, y, w, h;
    const char *label;      /* main legend */
    const char *sub;        /* second legend (PH, PL, SH, SL, H, L, A...) */
} widget_t;

extern const widget_t PANEL_WIDGETS[];
extern const int PANEL_WIDGET_COUNT;

/* Returns the index of the control under (x, y) in the panel's logical
 * coordinates, or -1. */
int panel_hit(int x, int y);

/* Draws the complete panel into `fb` (which must be PANEL_W x PANEL_H).
 * `held` maps which controls are drawn as pressed: one byte per control,
 * indexed the same as PANEL_WIDGETS. */
void panel_draw(fb_t *fb, const umk_machine_t *m, const unsigned char *held);

#endif /* UMK80_PANEL_H */
