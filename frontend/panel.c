/* panel.c — véase panel.h. */

#include "panel.h"
#include "font5x7.h"

/* --- paleta ---------------------------------------------------------------
 * Medida sobre docs/ref/umk-80.jpg y docs/ref/umk-80-hello-world.jpg. */
#define C_PANEL      0xB9B2A5u
#define C_PANEL_HI   0xCAC4B7u
#define C_PANEL_LO   0x9A9488u
#define C_INK        0x393530u
#define C_INK_SOFT   0x5C574Fu
#define C_KEY        0x201E1Bu
#define C_KEY_HI     0x35322Eu
#define C_KEY_LO     0x121110u
#define C_KEY_TXT    0xE6E1D8u
#define C_BEZEL      0xB0AEA8u
#define C_BEZEL_HI   0xD2D0C9u
#define C_BEZEL_LO   0x807E79u
#define C_WINDOW     0x360F0Au
/* En el equipo real los segmentos apagados apenas se adivinan tras el filtro
 * ahumado; si se dibujan demasiado vivos, seis «8» fantasma compiten con lo
 * que de verdad está encendido. */
#define C_SEG_OFF    0x2B0C08u
#define C_LED_OFF    0x431310u
#define C_LED_RIM    0x2A1512u
#define C_SLOT       0x8F897Eu

static uint32_t mix(uint32_t a, uint32_t b, unsigned t /* 0..255 */)
{
    unsigned ar = (a >> 16) & 0xFFu, ag = (a >> 8) & 0xFFu, ab = a & 0xFFu;
    unsigned br = (b >> 16) & 0xFFu, bg = (b >> 8) & 0xFFu, bb = b & 0xFFu;
    unsigned r = (ar * (255u - t) + br * t) / 255u;
    unsigned g = (ag * (255u - t) + bg * t) / 255u;
    unsigned bl = (ab * (255u - t) + bb * t) / 255u;
    return (r << 16) | (g << 8) | bl;
}

/* --- primitivas ------------------------------------------------------------ */

static void px_set(fb_t *fb, int x, int y, uint32_t c)
{
    if (x >= 0 && y >= 0 && x < fb->w && y < fb->h) fb->px[y * fb->w + x] = c;
}

static void px_blend(fb_t *fb, int x, int y, uint32_t c, unsigned t)
{
    if (x >= 0 && y >= 0 && x < fb->w && y < fb->h) {
        uint32_t *p = &fb->px[y * fb->w + x];
        *p = mix(*p, c, t);
    }
}

static void fill(fb_t *fb, int x, int y, int w, int h, uint32_t c)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++) px_set(fb, i, j, c);
}

/* Rectángulo con biselado de un píxel: claro arriba/izquierda, oscuro
 * abajo/derecha (o al revés si `sunken`). */
static void plate(fb_t *fb, int x, int y, int w, int h,
                  uint32_t face, uint32_t hi, uint32_t lo, int sunken)
{
    uint32_t a = sunken ? lo : hi, b = sunken ? hi : lo;
    fill(fb, x, y, w, h, face);
    fill(fb, x, y, w, 1, a);
    fill(fb, x, y, 1, h, a);
    fill(fb, x, y + h - 1, w, 1, b);
    fill(fb, x + w - 1, y, 1, h, b);
}

static void disc(fb_t *fb, int cx, int cy, int r, uint32_t c)
{
    int x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r * r) px_set(fb, cx + x, cy + y, c);
            else if (d <= (r + 1) * (r + 1))
                px_blend(fb, cx + x, cy + y, c, 110u);   /* antialias tosco */
        }
}

/* Halo suave, para los LEDs y los segmentos encendidos. */
static void glow(fb_t *fb, int cx, int cy, int r, uint32_t c, unsigned strength)
{
    int x, y;
    for (y = -r; y <= r; y++)
        for (x = -r; x <= r; x++) {
            int d2 = x * x + y * y;
            if (d2 > r * r) continue;
            {
                unsigned t = (unsigned)((r * r - d2) * (int)strength / (r * r));
                px_blend(fb, cx + x, cy + y, c, t);
            }
        }
}

static void text(fb_t *fb, int x, int y, const char *s, int sc, uint32_t c)
{
    while (*s) {
        uint32_t cp = utf8_next(&s);
        const glyph_t *g = &FONT5X7[font_glyph(cp)];
        int r, k, i, j;
        for (r = 0; r < 7; r++)
            for (k = 0; k < 5; k++)
                if (g->rows[r] & (1u << (4 - k)))
                    for (j = 0; j < sc; j++)
                        for (i = 0; i < sc; i++)
                            px_set(fb, x + k * sc + i, y + r * sc + j, c);
        x += 6 * sc;
    }
}

static void text_c(fb_t *fb, int cx, int y, const char *s, int sc, uint32_t c)
{
    text(fb, cx - font_width(s, sc) / 2, y, s, sc, c);
}

/* --- distribución del panel ------------------------------------------------
 *
 * Coordenadas en el espacio lógico de PANEL_W x PANEL_H, siguiendo la
 * disposición del Рис. 2: АДРЕС arriba, ДАННЫЕ debajo, СОСТОЯНИЕ debajo de
 * esa, el display a la izquierda de las dos últimas, СБ y ПР al borde
 * derecho, los dos teclados abajo al centro y РБ/ШГ, КМ/ЦК y ШГ al borde
 * derecho más abajo.
 */
#define LED_R        7
#define ADR_Y        118
#define DAT_Y        180
#define STA_Y        252
#define LED_LABEL_DY 14

#define DISP_X   356
#define DISP_Y   206
#define DISP_W   248
#define DISP_H   96

#define KEY_W  50
#define KEY_H  54
#define KEY_G   7

#define DIR_X  366
#define HEX_X  512
#define PAD_Y  356

#define SIDE_X 916
#define SIDE_W 62

/* Las filas de la matriz, de arriba abajo en el panel, son los bits 4, 6, 5
 * y 2 de PORTC; en el núcleo esas filas se indexan 1, 3, 2 y 0. */
static const int ROW_ORDER[4] = { 1, 3, 2, 0 };

#define W_(k, c, r, X, Y, W, H, L, S) { k, c, r, X, Y, W, H, L, S }

const widget_t PANEL_WIDGETS[] = {
    /* --- teclas directivas: 2 columnas x 4 filas --------------------------- */
    W_(W_KEY, 0, 1, DIR_X,                  PAD_Y,                    KEY_W, KEY_H, "П",  NULL),
    W_(W_KEY, 1, 1, DIR_X + KEY_W + KEY_G,  PAD_Y,                    KEY_W, KEY_H, "РГ", NULL),
    W_(W_KEY, 0, 3, DIR_X,                  PAD_Y + (KEY_H + KEY_G),  KEY_W, KEY_H, "СТ", NULL),
    W_(W_KEY, 1, 3, DIR_X + KEY_W + KEY_G,  PAD_Y + (KEY_H + KEY_G),  KEY_W, KEY_H, "КС", NULL),
    W_(W_KEY, 0, 2, DIR_X,                  PAD_Y + 2*(KEY_H+KEY_G),  KEY_W, KEY_H, "ЗК", NULL),
    W_(W_KEY, 1, 2, DIR_X + KEY_W + KEY_G,  PAD_Y + 2*(KEY_H+KEY_G),  KEY_W, KEY_H, "ПМ", NULL),
    W_(W_KEY, 0, 0, DIR_X,                  PAD_Y + 3*(KEY_H+KEY_G),  KEY_W, KEY_H, "_",  NULL),
    W_(W_KEY, 1, 0, DIR_X + KEY_W + KEY_G,  PAD_Y + 3*(KEY_H+KEY_G),  KEY_W, KEY_H, "ВП", NULL),

    /* --- teclado hexadecimal: 4 columnas x 4 filas -------------------------
     * Los segundos rótulos son los identificadores de registro de la
     * directiva РГ, y coinciden uno a uno con la tabla TBLRG del monitor. */
    W_(W_KEY, 2, 1, HEX_X,                    PAD_Y,                   KEY_W, KEY_H, "0", NULL),
    W_(W_KEY, 3, 1, HEX_X + (KEY_W+KEY_G),    PAD_Y,                   KEY_W, KEY_H, "1", NULL),
    W_(W_KEY, 4, 1, HEX_X + 2*(KEY_W+KEY_G),  PAD_Y,                   KEY_W, KEY_H, "2", NULL),
    W_(W_KEY, 5, 1, HEX_X + 3*(KEY_W+KEY_G),  PAD_Y,                   KEY_W, KEY_H, "3", NULL),

    W_(W_KEY, 2, 3, HEX_X,                    PAD_Y + (KEY_H+KEY_G),   KEY_W, KEY_H, "4", "PH"),
    W_(W_KEY, 3, 3, HEX_X + (KEY_W+KEY_G),    PAD_Y + (KEY_H+KEY_G),   KEY_W, KEY_H, "5", "PL"),
    W_(W_KEY, 4, 3, HEX_X + 2*(KEY_W+KEY_G),  PAD_Y + (KEY_H+KEY_G),   KEY_W, KEY_H, "6", "SH"),
    W_(W_KEY, 5, 3, HEX_X + 3*(KEY_W+KEY_G),  PAD_Y + (KEY_H+KEY_G),   KEY_W, KEY_H, "7", "SL"),

    W_(W_KEY, 2, 2, HEX_X,                    PAD_Y + 2*(KEY_H+KEY_G), KEY_W, KEY_H, "8", "H"),
    W_(W_KEY, 3, 2, HEX_X + (KEY_W+KEY_G),    PAD_Y + 2*(KEY_H+KEY_G), KEY_W, KEY_H, "9", "L"),
    W_(W_KEY, 4, 2, HEX_X + 2*(KEY_W+KEY_G),  PAD_Y + 2*(KEY_H+KEY_G), KEY_W, KEY_H, "A", "A"),
    W_(W_KEY, 5, 2, HEX_X + 3*(KEY_W+KEY_G),  PAD_Y + 2*(KEY_H+KEY_G), KEY_W, KEY_H, "B", "B"),

    W_(W_KEY, 2, 0, HEX_X,                    PAD_Y + 3*(KEY_H+KEY_G), KEY_W, KEY_H, "C", "C"),
    W_(W_KEY, 3, 0, HEX_X + (KEY_W+KEY_G),    PAD_Y + 3*(KEY_H+KEY_G), KEY_W, KEY_H, "D", "D"),
    W_(W_KEY, 4, 0, HEX_X + 2*(KEY_W+KEY_G),  PAD_Y + 3*(KEY_H+KEY_G), KEY_W, KEY_H, "E", "E"),
    W_(W_KEY, 5, 0, HEX_X + 3*(KEY_W+KEY_G),  PAD_Y + 3*(KEY_H+KEY_G), KEY_W, KEY_H, "F", "F"),

    /* --- borde derecho: pulsadores y conmutadores -------------------------- */
    W_(W_BTN_SB,   -1, -1, SIDE_X, 150, SIDE_W, 44, "СБ",  NULL),
    W_(W_BTN_PR,   -1, -1, SIDE_X, 202, SIDE_W, 44, "ПР",  NULL),
    W_(W_SW_RBSHG, -1, -1, SIDE_X, 330, SIDE_W, 52, "РБ",  "ШГ"),
    W_(W_SW_KMCK,  -1, -1, SIDE_X, 392, SIDE_W, 52, "КМ",  "ЦК"),
    W_(W_BTN_SHG,  -1, -1, SIDE_X, 500, SIDE_W, 44, "ШГ",  NULL)
};

const int PANEL_WIDGET_COUNT =
    (int)(sizeof PANEL_WIDGETS / sizeof PANEL_WIDGETS[0]);

int panel_hit(int x, int y)
{
    int i;
    for (i = 0; i < PANEL_WIDGET_COUNT; i++) {
        const widget_t *w = &PANEL_WIDGETS[i];
        if (x >= w->x && x < w->x + w->w && y >= w->y && y < w->y + w->h)
            return i;
    }
    return -1;
}

/* --- indicadores de siete segmentos ---------------------------------------
 *
 * Seis dígitos agrupados 4 + 2, con un hueco mayor entre el cuarto y el
 * quinto: el campo АДРЕС y el campo ДАННЫЕ. En el equipo real ese hueco se
 * ve a simple vista (docs/ref/umk-80-hello-world.jpg, donde «HELLO» sale
 * como «HELL O»).
 */
#define DIG_W   28
#define DIG_H   48
#define DIG_GAP  6
#define DIG_SPLIT 18
#define SLANT     4      /* inclinación tipo АЛС, en píxeles sobre la altura */

/* Rectángulo del segmento en coordenadas del dígito: x, y, ancho, alto. */
static const signed char SEG_BOX[7][4] = {
    {  4,  0, 20,  5 },   /* A  superior       */
    { 23,  3,  5, 21 },   /* B  sup. derecho   */
    { 23, 24,  5, 21 },   /* C  inf. derecho   */
    {  4, 43, 20,  5 },   /* D  inferior       */
    {  0, 24,  5, 21 },   /* E  inf. izquierdo */
    {  0,  3,  5, 21 },   /* F  sup. izquierdo */
    {  4, 21, 20,  5 }    /* G  central        */
};

static void draw_digit(fb_t *fb, int ox, int oy, const uint8_t inten[UMK_SEGMENTS])
{
    int s, i, j;
    for (s = 0; s < 7; s++) {
        /* El brillo absoluto de un display multiplexado a seis dígitos no
         * pasa de 255/6; se reescala para que la pantalla se vea como en el
         * equipo real, conservando las diferencias relativas (que es donde
         * se manifiesta el fantasmeo). */
        unsigned v = inten[s] * 6u;
        uint32_t c;
        if (v > 255u) v = 255u;
        c = mix(C_SEG_OFF, 0xFF4432u, (unsigned)v);

        for (j = 0; j < SEG_BOX[s][3]; j++) {
            int yy = oy + SEG_BOX[s][1] + j;
            int sh = ((DIG_H - (SEG_BOX[s][1] + j)) * SLANT) / DIG_H;
            for (i = 0; i < SEG_BOX[s][2]; i++)
                px_set(fb, ox + SEG_BOX[s][0] + i + sh, yy, c);
        }
        if (v > 60u) {
            int cx = ox + SEG_BOX[s][0] + SEG_BOX[s][2] / 2 + SLANT / 2;
            int cy = oy + SEG_BOX[s][1] + SEG_BOX[s][3] / 2;
            glow(fb, cx, cy, 12, 0xFF5040u, v / 5u);
        }
    }
    /* punto decimal: bit 7 */
    {
        unsigned v = inten[7] * 6u;
        if (v > 255u) v = 255u;
        disc(fb, ox + 30, oy + DIG_H - 2, 2, mix(C_SEG_OFF, 0xFF4432u, (unsigned)v));
    }
}

static void draw_display(fb_t *fb, const umk_machine_t *m)
{
    uint8_t inten[UMK_DIGITS][UMK_SEGMENTS];
    int i, x;

    umk_display_intensity(m, inten);

    /* bisel cromado y ventana ahumada */
    plate(fb, DISP_X - 6, DISP_Y - 6, DISP_W + 12, DISP_H + 12,
          C_BEZEL, C_BEZEL_HI, C_BEZEL_LO, 0);
    plate(fb, DISP_X, DISP_Y, DISP_W, DISP_H, C_WINDOW, C_BEZEL_LO, C_BEZEL_HI, 1);

    x = DISP_X + 14;
    for (i = 0; i < (int)UMK_DIGITS; i++) {
        draw_digit(fb, x, DISP_Y + 24, inten[i]);
        x += DIG_W + DIG_GAP;
        if (i == 3) x += DIG_SPLIT;   /* separación АДРЕС | ДАННЫЕ */
    }
}

/* --- filas de LEDs --------------------------------------------------------- */

static void draw_led(fb_t *fb, int cx, int cy, int on)
{
    disc(fb, cx, cy, LED_R + 1, C_LED_RIM);
    if (on) {
        glow(fb, cx, cy, LED_R * 3, 0xFF2A18u, 150u);
        disc(fb, cx, cy, LED_R, 0xFF3A22u);
        disc(fb, cx - 2, cy - 2, 2, 0xFFC0A0u);
    } else {
        disc(fb, cx, cy, LED_R, C_LED_OFF);
        disc(fb, cx - 2, cy - 2, 2, mix(C_LED_OFF, C_PANEL_HI, 60u));
    }
}

/* Fila de `n` LEDs desde la derecha (bit 0 el de más a la derecha), agrupados
 * de cuatro en cuatro como en el equipo real. El paso se pasa como parámetro
 * porque la fila СОСТОЯНИЕ lleva rótulos largos (STACK, HLTA, MEMR) y
 * necesita más sitio que las de АДРЕС y ДАННЫЕ. */
static void draw_led_row(fb_t *fb, int right_x, int y, unsigned n,
                         uint32_t value, const char *const *labels,
                         int pitch, int group)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        int gx = (int)(i / 4u) * group;
        int cx = right_x - (int)i * pitch - gx;
        draw_led(fb, cx, y, (int)((value >> i) & 1u));
        if (labels) text_c(fb, cx, y + LED_LABEL_DY, labels[i], 1, C_INK_SOFT);
    }
}

static const char *const LBL_ADDR[16] = {
    "0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"
};
static const char *const LBL_DATA[8] = { "0","1","2","3","4","5","6","7" };
/* Rótulos de la palabra de estado, de bit 0 a bit 7. En el panel se leen de
 * izquierda a derecha como MEMR INP M1 OUT HLTA STACK WO INTA, o sea del
 * bit 7 al bit 0. */
static const char *const LBL_STAT[8] = {
    "INTA","WO","STACK","HLTA","OUT","M1","INP","MEMR"
};

/* --- teclas y conmutadores -------------------------------------------------- */

static void draw_key(fb_t *fb, const widget_t *w, int pressed, int latched)
{
    int y = w->y + (pressed ? 2 : 0);
    int h = w->h - (pressed ? 2 : 0);

    if (!pressed) fill(fb, w->x + 2, w->y + 4, w->w, h, mix(C_PANEL, 0x000000u, 60u));
    plate(fb, w->x, y, w->w, h, pressed ? C_KEY_LO : C_KEY,
          C_KEY_HI, C_KEY_LO, pressed);

    if (latched) {   /* conmutador enclavado: testigo claro en el borde */
        fill(fb, w->x + 3, y + 3, w->w - 6, 3, 0xC8B060u);
    }

    if (w->sub) {
        text_c(fb, w->x + w->w / 2, y + h / 2 - 12, w->label, 2, C_KEY_TXT);
        text_c(fb, w->x + w->w / 2, y + h / 2 + 4,  w->sub,   2, C_KEY_TXT);
    } else {
        text_c(fb, w->x + w->w / 2, y + h / 2 - 7, w->label, 2, C_KEY_TXT);
    }
}

/* --- adornos del panel ------------------------------------------------------ */

static void draw_vents(fb_t *fb)
{
    int i;
    for (i = 0; i < 9; i++) {
        int yy = 40 + i * 13;
        fill(fb, 40, yy, 200, 5, C_SLOT);
        fill(fb, 40, yy + 5, 200, 1, C_PANEL_HI);
    }
}

static void draw_handle(fb_t *fb)
{
    fill(fb, 280, 110, 58, 410, mix(C_PANEL, 0x000000u, 40u));
    plate(fb, 284, 118, 50, 394, C_BEZEL, C_BEZEL_HI, C_BEZEL_LO, 0);
    plate(fb, 292, 150, 34, 330, mix(C_BEZEL, C_PANEL, 120u),
          C_BEZEL_HI, C_BEZEL_LO, 1);
}

static void draw_logo(fb_t *fb)
{
    plate(fb, 56, 290, 48, 26, C_PANEL_LO, C_PANEL_HI, C_INK, 1);
    text_c(fb, 80, 296, "ВЭФ", 2, C_INK);
    text(fb, 116, 288, "УМК", 5, C_INK);
    text(fb, 56, 326, "УЧЕБНЫЙ", 1, C_INK_SOFT);
    text(fb, 56, 338, "МИКРОПРОЦЕССОРНЫЙ", 1, C_INK_SOFT);
    text(fb, 56, 350, "КОМПЛЕКТ", 1, C_INK_SOFT);
}

/* Los tres LEDs de alimentación son indicadores de AVERÍA: encendido
 * significa que esa tensión FALTA (ПС §4.3 «устройство индикации аварии»;
 * ver DESCONOCIDOS.md §8). */
static void draw_power(fb_t *fb, const umk_machine_t *m)
{
    static const char *const L[3] = { "+5V", "-5V", "+12V" };
    const int faults[3] = { m->panel.fault_p5, m->panel.fault_m5, m->panel.fault_p12 };
    int i;
    text(fb, 56, 430, "ИНДИКАЦИЯ АВАРИИ", 1, C_INK_SOFT);
    for (i = 0; i < 3; i++) {
        int cx = 70 + i * 54;
        draw_led(fb, cx, 460, faults[i]);
        text_c(fb, cx, 476, L[i], 1, C_INK_SOFT);
    }
    text(fb, 56, 512, "СЕТЬ", 1, C_INK_SOFT);
    plate(fb, 56, 524, 40, 34, C_KEY, C_KEY_HI, C_KEY_LO, 0);
    text_c(fb, 76, 535, "~", 2, C_KEY_TXT);
}

/* --- panel completo --------------------------------------------------------- */

void panel_draw(fb_t *fb, const umk_machine_t *m, const unsigned char *held)
{
    int i;

    fill(fb, 0, 0, fb->w, fb->h, C_PANEL);
    /* Un degradado muy suave para que no parezca cartón. */
    for (i = 0; i < fb->h; i++) {
        unsigned t = (unsigned)(i * 40 / fb->h);
        int x;
        for (x = 0; x < fb->w; x++)
            fb->px[i * fb->w + x] = mix(C_PANEL_HI, C_PANEL_LO, t + 90u);
    }

    draw_vents(fb);
    draw_handle(fb);
    draw_logo(fb);
    draw_power(fb, m);

    /* filas de LEDs con sus títulos */
    text_c(fb, 718, ADR_Y - 28, "АДРЕС", 2, C_INK);
    draw_led_row(fb, 900, ADR_Y, 16, m->panel.address, LBL_ADDR, 22, 12);

    text_c(fb, 812, DAT_Y - 28, "ДАННЫЕ", 2, C_INK);
    draw_led_row(fb, 900, DAT_Y, 8, m->panel.data, LBL_DATA, 22, 12);

    text_c(fb, 750, STA_Y - 28, "СОСТОЯНИЕ", 2, C_INK);
    draw_led_row(fb, 898, STA_Y, 8, m->panel.status, LBL_STAT, 38, 10);

    draw_display(fb, m);

    text(fb, DIR_X, PAD_Y - 18, "ДИРЕКТИВНЫЕ", 1, C_INK_SOFT);
    text(fb, HEX_X, PAD_Y - 18, "ИНФОРМАЦИОННЫЕ", 1, C_INK_SOFT);

    for (i = 0; i < PANEL_WIDGET_COUNT; i++) {
        const widget_t *w = &PANEL_WIDGETS[i];
        int latched = 0;
        if (w->kind == W_SW_RBSHG) latched = umk_get_switch(m, UMK_SW_STEP);
        if (w->kind == W_SW_KMCK)  latched = umk_get_switch(m, UMK_SW_CYCLE);
        draw_key(fb, w, held && held[i], latched);
    }

    /* Nota discreta: el modo paso a paso está activo. */
    if (umk_get_switch(m, UMK_SW_STEP)) {
        const char *s = umk_get_switch(m, UMK_SW_CYCLE)
                      ? "ПОШАГОВЫЙ РЕЖИМ - ЦИКЛ" : "ПОШАГОВЫЙ РЕЖИМ - КОМАНДА";
        text(fb, DISP_X, DISP_Y + DISP_H + 22, s, 1, C_INK);
    }
}

/* Índices de los controles auxiliares en ROW_ORDER, referenciados aquí para
 * que el compilador no avise de variable sin usar cuando cambie el diseño. */
const int *panel_row_order(void) { return ROW_ORDER; }
