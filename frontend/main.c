/* main.c — frontend gráfico del УМК-80.
 *
 * Junta el núcleo (que no sabe nada de ventanas) con el dibujo del panel y
 * con la capa de sistema. Se opera con el ratón sobre las teclas dibujadas o
 * con el teclado del anfitrión.
 */

#include "panel.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- correspondencia con el teclado del anfitrión --------------------------
 *
 * Los dieciséis dígitos hexadecimales van en sus propias teclas. Las seis
 * directivas, que en el equipo son teclas rotuladas en cirílico, van en F1
 * a F6 en el mismo orden en que las numera el monitor (códigos 0 a 5 de
 * CTBL): П РГ СТ КС ЗК ПМ.
 */
typedef struct { int key; int col, row; } keymap_t;

#define R_B2 0
#define R_B4 1
#define R_B5 2
#define R_B6 3

static const keymap_t KEYMAP[] = {
    /* dígitos */
    { '0', 2, R_B4 }, { '1', 3, R_B4 }, { '2', 4, R_B4 }, { '3', 5, R_B4 },
    { '4', 2, R_B6 }, { '5', 3, R_B6 }, { '6', 4, R_B6 }, { '7', 5, R_B6 },
    { '8', 2, R_B5 }, { '9', 3, R_B5 }, { 'A', 4, R_B5 }, { 'B', 5, R_B5 },
    { 'C', 2, R_B2 }, { 'D', 3, R_B2 }, { 'E', 4, R_B2 }, { 'F', 5, R_B2 },
    /* directivas */
    { PK_F1 + 0, 0, R_B4 },   /* П  */
    { PK_F1 + 1, 1, R_B4 },   /* РГ */
    { PK_F1 + 2, 0, R_B6 },   /* СТ */
    { PK_F1 + 3, 1, R_B6 },   /* КС */
    { PK_F1 + 4, 0, R_B5 },   /* ЗК */
    { PK_F1 + 5, 1, R_B5 },   /* ПМ */
    /* separador y fin de directiva */
    { ' ',       0, R_B2 },   /* пробел */
    { PK_ENTER,  1, R_B2 }    /* ВП     */
};
#define KEYMAP_N ((int)(sizeof KEYMAP / sizeof KEYMAP[0]))

/* --- estado del frontend ---------------------------------------------------- */

static umk_machine_t   machine;
static uint32_t        framebuffer[PANEL_W * PANEL_H];
static unsigned char   held[64];        /* controles dibujados pulsados */
static int             mouse_widget = -1;

static int widget_of(int col, int row)
{
    int i;
    for (i = 0; i < PANEL_WIDGET_COUNT; i++)
        if (PANEL_WIDGETS[i].kind == W_KEY &&
            PANEL_WIDGETS[i].col == col && PANEL_WIDGETS[i].row == row)
            return i;
    return -1;
}

static void key_action(int widget, int down)
{
    const widget_t *w;
    if (widget < 0 || widget >= PANEL_WIDGET_COUNT) return;
    w = &PANEL_WIDGETS[widget];
    held[widget] = (unsigned char)(down ? 1 : 0);

    switch (w->kind) {
        case W_KEY:
            umk_set_key(&machine, (unsigned)w->col, (unsigned)w->row, down != 0);
            break;
        case W_BTN_SB:
            if (down) umk_reset(&machine);
            break;
        case W_BTN_PR:
            if (down) umk_interrupt(&machine);
            break;
        case W_BTN_SHG:
            if (down) umk_press_step(&machine);
            break;
        case W_SW_RBSHG:
            if (down) {   /* conmutador CON enclavamiento: alterna */
                umk_set_switch(&machine, UMK_SW_STEP,
                               !umk_get_switch(&machine, UMK_SW_STEP));
            }
            held[widget] = 0;
            break;
        case W_SW_KMCK:
            if (down) {
                umk_set_switch(&machine, UMK_SW_CYCLE,
                               !umk_get_switch(&machine, UMK_SW_CYCLE));
            }
            held[widget] = 0;
            break;
        default:
            break;
    }
}

static void host_key(int key, int down)
{
    int i;
    for (i = 0; i < KEYMAP_N; i++) {
        if (KEYMAP[i].key == key) {
            key_action(widget_of(KEYMAP[i].col, KEYMAP[i].row), down);
            return;
        }
    }
    switch (key) {
        case PK_ESC:      /* СБ */
            for (i = 0; i < PANEL_WIDGET_COUNT; i++)
                if (PANEL_WIDGETS[i].kind == W_BTN_SB) key_action(i, down);
            break;
        case PK_BACK:     /* ПР */
            for (i = 0; i < PANEL_WIDGET_COUNT; i++)
                if (PANEL_WIDGETS[i].kind == W_BTN_PR) key_action(i, down);
            break;
        case PK_F1 + 7:   /* F8  = ШГ    */
            for (i = 0; i < PANEL_WIDGET_COUNT; i++)
                if (PANEL_WIDGETS[i].kind == W_BTN_SHG) key_action(i, down);
            break;
        case PK_F1 + 8:   /* F9  = РБ/ШГ */
            if (down) for (i = 0; i < PANEL_WIDGET_COUNT; i++)
                if (PANEL_WIDGETS[i].kind == W_SW_RBSHG) key_action(i, 1);
            break;
        case PK_F1 + 9:   /* F10 = КМ/ЦК */
            if (down) for (i = 0; i < PANEL_WIDGET_COUNT; i++)
                if (PANEL_WIDGETS[i].kind == W_SW_KMCK) key_action(i, 1);
            break;
        default:
            break;
    }
}

/* --- carga de la ROM --------------------------------------------------------- */

static int load_rom_file(const char *path, uint16_t offset)
{
    static uint8_t buf[UMK_ROM_MAX];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (n == 0) return 0;
    return umk_load_rom(&machine, offset, buf, n) ? (int)n : 0;
}

/* --- bucle principal ---------------------------------------------------------- */

/* Vuelca el framebuffer como PPM binario (P6). Sirve para mirar el panel sin
 * abrir ventana y para comparar dibujados entre versiones. */
static int write_ppm(const char *path, const uint32_t *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (i = 0; i < w * h; i++) {
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((px[i] >> 16) & 0xFFu);
        rgb[1] = (unsigned char)((px[i] >> 8) & 0xFFu);
        rgb[2] = (unsigned char)(px[i] & 0xFFu);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    const char *rompath = "rom/monitor.bin";
    const char *shot = NULL;
    const char *script = NULL;
    fb_t fb;
    uint64_t t_prev;
    int running = 1;
    int i, n;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) rompath = argv[++i];
        else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot = argv[++i];
        else if (strcmp(argv[i], "--keys") == 0 && i + 1 < argc) script = argv[++i];
        else if (strcmp(argv[i], "--help") == 0) {
            printf("uso: %s [--rom <fichero>]\n\n"
                   "teclado del anfitrión:\n"
                   "  0-9 A-F   teclas hexadecimales\n"
                   "  F1..F6    П РГ СТ КС ЗК ПМ\n"
                   "  espacio   separador de parámetros\n"
                   "  intro     ВП (fin de directiva)\n"
                   "  esc       СБ (сброс)\n"
                   "  retroceso ПР (прерывание)\n"
                   "  F8        ШГ (шаг)\n"
                   "  F9        РБ/ШГ (enclava el modo paso a paso)\n"
                   "  F10       КМ/ЦК (paso por ciclo de máquina)\n", argv[0]);
            return 0;
        }
    }

    umk_init(&machine, UMK_REV2);
    n = load_rom_file(rompath, 0);
    if (!n) {
        fprintf(stderr, "no se pudo cargar el ПЗУ «%s».\n"
                        "Genéralo con:  make rom/monitor.bin\n", rompath);
        return 2;
    }
    printf("ПЗУ cargado: %s (%d bytes)\n", rompath, n);
    umk_reset(&machine);

    fb.px = framebuffer; fb.w = PANEL_W; fb.h = PANEL_H;

    /* Modo sin ventana: teclea un guion, deja correr y vuelca el panel.
     * El guion usa los mismos códigos que el teclado del anfitrión, más
     * '>' para ВП y '.' para el separador. */
    if (shot) {
        if (script) {
            const char *p;
            for (p = script; *p; p++) {
                int k = *p;
                if (k == '>') k = PK_ENTER;
                else if (k == '.') k = ' ';
                else if (k == '!') k = PK_ESC;
                else if (k == '~') k = PK_BACK;
                else if (k >= 'p' && k <= 'u') k = PK_F1 + (k - 'p');
                host_key(k, 1);
                umk_run_cycles(&machine, 120000u);
                host_key(k, 0);
                umk_run_cycles(&machine, 120000u);
            }
        } else {
            umk_run_cycles(&machine, 400000u);
        }
        umk_display_clear_accumulator(&machine);
        umk_run_cycles(&machine, 200000u);
        panel_draw(&fb, &machine, held);
        if (!write_ppm(shot, framebuffer, PANEL_W, PANEL_H)) {
            fprintf(stderr, "no se pudo escribir %s\n", shot);
            return 2;
        }
        printf("panel volcado en %s\n", shot);
        return 0;
    }

    if (!plat_open("УМК-80 — ВЭФ РР3.059.004", PANEL_W, PANEL_H, 1, 1)) {
        fprintf(stderr, "no se pudo abrir la ventana\n");
        return 2;
    }

    t_prev = plat_ticks_ms();

    while (running) {
        plat_event_t ev;
        uint64_t t_now, dt;

        while (plat_poll(&ev)) {
            switch (ev.kind) {
                case EV_QUIT:
                    running = 0;
                    break;
                case EV_KEYDOWN:
                    host_key(ev.key, 1);
                    break;
                case EV_KEYUP:
                    host_key(ev.key, 0);
                    break;
                case EV_MOUSEDOWN:
                    mouse_widget = panel_hit(ev.x, ev.y);
                    if (mouse_widget >= 0) key_action(mouse_widget, 1);
                    break;
                case EV_MOUSEUP:
                    if (mouse_widget >= 0) key_action(mouse_widget, 0);
                    mouse_widget = -1;
                    break;
                default:
                    break;
            }
        }

        /* Avanzar el tiempo simulado a 2 MHz, con un tope para que un
         * parón del anfitrión no dispare una avalancha de ciclos. */
        t_now = plat_ticks_ms();
        dt = t_now - t_prev;
        t_prev = t_now;
        if (dt > 100u) dt = 100u;
        if (dt > 0u) umk_run_cycles(&machine, (machine.clock_hz / 1000u) * dt);

        panel_draw(&fb, &machine, held);
        plat_present(framebuffer);
        plat_sleep_ms(8);
    }

    plat_close();
    return 0;
}
