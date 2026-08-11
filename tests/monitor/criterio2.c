/* criterio2.c — criterio de aceptación 2 del encargo.
 *
 *   СБ, П, 0800, _, 3E, _, AA, _, C3, _, 00, _, 08, _, ВП
 *   СТ, 0800, ВП        -> el programa corre en bucle
 *   ПР                  -> lo interrumpe
 *   РГ, A               -> el display muestra "A - AA"
 *
 * Todo se hace pulsando teclas de la matriz real y leyendo el display real:
 * no se toca la memoria por detrás ni se llama a ninguna rutina del monitor.
 */

#include "umk80/umk80.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- teclado del УМК-80 --------------------------------------------------
 *
 * Matriz 6 columnas (bits de PORTA) x 4 filas (bits 2,4,5,6 de PORTC),
 * deducida de la rutina CONV del monitor (МОН hojas −31− y −32−):
 *
 *   - columnas 0 y 1 (PORTA bits 0,1) -> teclas de directiva
 *   - columnas 2..5  (PORTA bits 2..5) -> dígitos hexadecimales
 *   - el código de fila vale 0, 4, 8 o 12 para los bits 4, 6, 5 y 2
 *
 *   dígito   = (columna - 2) + código_de_fila
 *   función  = (columna) + código_de_fila / 2
 *
 * Las filas se numeran aquí 0..3 = bits 2, 4, 5, 6 de PORTC, igual que en
 * el núcleo (umk80.h, ROW_BIT).
 */
#define ROW_B2 0u
#define ROW_B4 1u
#define ROW_B5 2u
#define ROW_B6 3u

typedef struct { const char *name; unsigned col, row; } key_t;

static const key_t KEY_DIGIT[16] = {
    {"0", 2, ROW_B4}, {"1", 3, ROW_B4}, {"2", 4, ROW_B4}, {"3", 5, ROW_B4},
    {"4", 2, ROW_B6}, {"5", 3, ROW_B6}, {"6", 4, ROW_B6}, {"7", 5, ROW_B6},
    {"8", 2, ROW_B5}, {"9", 3, ROW_B5}, {"A", 4, ROW_B5}, {"B", 5, ROW_B5},
    {"C", 2, ROW_B2}, {"D", 3, ROW_B2}, {"E", 4, ROW_B2}, {"F", 5, ROW_B2}
};

static const key_t KEY_P     = {"П",  0, ROW_B4};   /* código 0 -> REPLM  */
static const key_t KEY_RG    = {"РГ", 1, ROW_B4};   /* código 1 -> REPLRG */
static const key_t KEY_ST    = {"СТ", 0, ROW_B6};   /* código 2 -> GOTO   */
static const key_t KEY_SPACE = {"_",  0, ROW_B2};   /* código 6 -> SPACE  */
static const key_t KEY_VP    = {"ВП", 1, ROW_B2};   /* código 7 -> CR     */

/* --- utilidades ----------------------------------------------------------- */

static const char *SEGNAME = "ABCDEFG.";

static void show_display(umk_machine_t *m, const char *label)
{
    uint8_t pat[UMK_DIGITS];
    unsigned i, s;
    umk_display_pattern(m, 25u, pat);
    printf("  %-22s", label);
    for (i = 0; i < UMK_DIGITS; i++) printf(" %02X", pat[i]);
    printf("   ");
    for (i = 0; i < UMK_DIGITS; i++) {
        printf("[");
        for (s = 0; s < 7u; s++) putchar((pat[i] & (1u << s)) ? SEGNAME[s] : '-');
        printf("]");
    }
    putchar('\n');
}

/* El monitor antirrebota por software con un retardo de 10 ms (TIME = 850),
 * y luego espera a que la tecla se suelte. Hay que mantenerla pulsada más de
 * ese tiempo y soltarla bien. A 2 MHz, 10 ms son 20000 ciclos T. */
#define HOLD_CYCLES    120000u
#define RELEASE_CYCLES 120000u

static void press(umk_machine_t *m, key_t k)
{
    umk_set_key(m, k.col, k.row, true);
    umk_run_cycles(m, HOLD_CYCLES);
    umk_set_key(m, k.col, k.row, false);
    umk_run_cycles(m, RELEASE_CYCLES);
}

static void press_hex(umk_machine_t *m, const char *digits)
{
    const char *p;
    for (p = digits; *p; p++) {
        int v = (*p >= '0' && *p <= '9') ? *p - '0' : (*p - 'A' + 10);
        press(m, KEY_DIGIT[v]);
    }
}

static int load_rom(umk_machine_t *m, const char *path)
{
    FILE *f = fopen(path, "rb");
    static uint8_t buf[UMK_ROM_MAX];
    size_t n;
    if (!f) { perror(path); return 0; }
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    printf("ПЗУ: %s (%lu bytes)\n", path, (unsigned long)n);
    return umk_load_rom(m, 0, buf, n);
}

/* --- prueba ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    static umk_machine_t m;
    const char *rom = (argc > 1) ? argv[1] : "rom/monitor.bin";
    uint8_t pat[UMK_DIGITS];
    int ok = 1;

    /* "  A - AA": posiciones 0 y 1 apagadas, 'A' identificador, '-'
     * separador, y el valor AA en las dos posiciones de datos.
     * 77h = 'A' en la tabla SMBTBL; 40h = sólo el segmento G = "-". */
    static const uint8_t EXPECTED[UMK_DIGITS] = { 0x00, 0x00, 0x77, 0x40, 0x77, 0x77 };

    printf("=== Criterio 2: monitor real, teclado real, display real ===\n\n");

    umk_init(&m, UMK_REV2);
    if (!load_rom(&m, rom)) { fprintf(stderr, "no se pudo cargar el ПЗУ\n"); return 2; }

    /* СБ */
    umk_reset(&m);
    umk_run_cycles(&m, 400000u);
    show_display(&m, "tras СБ");

    /* П 0800 _ 3E _ AA _ C3 _ 00 _ 08 _ ВП */
    press(&m, KEY_P);
    press_hex(&m, "0800");
    press(&m, KEY_SPACE);
    show_display(&m, "П 0800 _");

    press_hex(&m, "3E"); press(&m, KEY_SPACE);
    press_hex(&m, "AA"); press(&m, KEY_SPACE);
    press_hex(&m, "C3"); press(&m, KEY_SPACE);
    press_hex(&m, "00"); press(&m, KEY_SPACE);
    press_hex(&m, "08"); press(&m, KEY_SPACE);
    press(&m, KEY_VP);
    show_display(&m, "tras la carga");

    printf("\n  memoria 0800..0804: %02X %02X %02X %02X %02X\n",
           umk_peek(&m, 0x0800), umk_peek(&m, 0x0801), umk_peek(&m, 0x0802),
           umk_peek(&m, 0x0803), umk_peek(&m, 0x0804));
    if (umk_peek(&m, 0x0800) != 0x3E || umk_peek(&m, 0x0801) != 0xAA ||
        umk_peek(&m, 0x0802) != 0xC3 || umk_peek(&m, 0x0803) != 0x00 ||
        umk_peek(&m, 0x0804) != 0x08) {
        printf("  FALLO: el programa no quedó bien cargado\n");
        ok = 0;
    } else {
        printf("  OK: MVI A,0AAH / JMP 0800H cargado por teclado\n");
    }

    /* СТ 0800 ВП */
    press(&m, KEY_ST);
    press_hex(&m, "0800");
    press(&m, KEY_VP);

    /* Correr el programa de usuario un rato. */
    umk_run_cycles(&m, 500000u);
    printf("\n  PC del usuario tras arrancar: %04X (A=%02X)\n", m.cpu.pc, m.cpu.a);
    if (m.cpu.pc < 0x0800u || m.cpu.pc > 0x0805u) {
        printf("  FALLO: no se transfirió el control a 0800H\n");
        ok = 0;
    } else {
        printf("  OK: el control está en el programa de usuario\n");
    }

    /* ПР */
    umk_interrupt(&m);
    umk_run_cycles(&m, 400000u);
    show_display(&m, "tras ПР");

    /* РГ A */
    press(&m, KEY_RG);
    press(&m, KEY_DIGIT[0xA]);
    umk_display_clear_accumulator(&m);
    umk_run_cycles(&m, 200000u);
    show_display(&m, "РГ A");

    umk_display_pattern(&m, 25u, pat);
    printf("\n  esperado             ");
    { unsigned i; for (i = 0; i < UMK_DIGITS; i++) printf(" %02X", EXPECTED[i]); }
    printf("   (\"  A - AA\")\n");

    if (memcmp(pat, EXPECTED, sizeof EXPECTED) != 0) {
        printf("\nFALLO: el display no muestra \"A - AA\"\n");
        ok = 0;
    } else {
        printf("\nOK: el display muestra \"A - AA\"\n");
    }

    printf("\n===== %s =====\n", ok ? "CRITERIO 2 CUMPLIDO" : "CRITERIO 2 NO CUMPLIDO");
    return ok ? 0 : 1;
}
