/* criterion2.c — acceptance criterion 2.
 *
 *   СБ, П, 0800, _, 3E, _, AA, _, C3, _, 00, _, 08, _, ВП
 *   СТ, 0800, ВП        -> the program runs in a loop
 *   ПР                  -> interrupts it
 *   РГ, A               -> the display shows "A - AA"
 *
 * Everything is done by pressing keys of the real matrix and reading the real
 * display: memory is not touched behind the scenes and no monitor routine is
 * called directly.
 */

#include "umk80/umk80.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- УМК-80 keyboard -----------------------------------------------------
 *
 * A 6-column (PORTA bits) x 4-row (PORTC bits 2,4,5,6) matrix, derived from
 * the monitor's CONV routine (МОН sheets −31− and −32−):
 *
 *   - columns 0 and 1 (PORTA bits 0,1) -> directive keys
 *   - columns 2..5    (PORTA bits 2..5) -> hexadecimal digits
 *   - the row code is 0, 4, 8 or 12 for bits 4, 6, 5 and 2
 *
 *   digit    = (column - 2) + row_code
 *   function = (column)     + row_code / 2
 *
 * Rows are numbered 0..3 here = PORTC bits 2, 4, 5, 6, the same as in the
 * core (umk80.h, ROW_BIT).
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

static const key_t KEY_P     = {"П",  0, ROW_B4};   /* code 0 -> REPLM  */
static const key_t KEY_RG    = {"РГ", 1, ROW_B4};   /* code 1 -> REPLRG */
static const key_t KEY_ST    = {"СТ", 0, ROW_B6};   /* code 2 -> GOTO   */
static const key_t KEY_SPACE = {"_",  0, ROW_B2};   /* code 6 -> SPACE  */
static const key_t KEY_VP    = {"ВП", 1, ROW_B2};   /* code 7 -> CR     */

/* --- helpers -------------------------------------------------------------- */

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

/* The monitor debounces in software with a 10 ms delay (TIME = 850) and then
 * waits for the key to be released. A key has to be held longer than that and
 * released cleanly. At 2 MHz, 10 ms is 20000 T states. */
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
    printf("ROM: %s (%lu bytes)\n", path, (unsigned long)n);
    return umk_load_rom(m, 0, buf, n);
}

/* --- the test -------------------------------------------------------------- */

int main(int argc, char **argv)
{
    static umk_machine_t m;
    const char *rom = (argc > 1) ? argv[1] : "rom/monitor.bin";
    uint8_t pat[UMK_DIGITS];
    int ok = 1;

    /* "  A - AA": positions 0 and 1 dark, 'A' as the identifier, '-' as the
     * separator, and the value AA in the two data positions.
     * 77h = 'A' in the SMBTBL table; 40h = segment G alone = "-". */
    static const uint8_t EXPECTED[UMK_DIGITS] = { 0x00, 0x00, 0x77, 0x40, 0x77, 0x77 };

    printf("=== Criterion 2: real monitor, real keyboard, real display ===\n\n");

    umk_init(&m, UMK_REV2);
    if (!load_rom(&m, rom)) { fprintf(stderr, "could not load the ROM\n"); return 2; }

    /* СБ */
    umk_reset(&m);
    umk_run_cycles(&m, 400000u);
    show_display(&m, "after СБ");

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
    show_display(&m, "after loading");

    printf("\n  memory 0800..0804: %02X %02X %02X %02X %02X\n",
           umk_peek(&m, 0x0800), umk_peek(&m, 0x0801), umk_peek(&m, 0x0802),
           umk_peek(&m, 0x0803), umk_peek(&m, 0x0804));
    if (umk_peek(&m, 0x0800) != 0x3E || umk_peek(&m, 0x0801) != 0xAA ||
        umk_peek(&m, 0x0802) != 0xC3 || umk_peek(&m, 0x0803) != 0x00 ||
        umk_peek(&m, 0x0804) != 0x08) {
        printf("  FAIL: the program was not loaded correctly\n");
        ok = 0;
    } else {
        printf("  OK: MVI A,0AAH / JMP 0800H entered from the keyboard\n");
    }

    /* СТ 0800 ВП */
    press(&m, KEY_ST);
    press_hex(&m, "0800");
    press(&m, KEY_VP);

    /* Let the user program run for a while. */
    umk_run_cycles(&m, 500000u);
    printf("\n  user PC after start: %04X (A=%02X)\n", m.cpu.pc, m.cpu.a);
    if (m.cpu.pc < 0x0800u || m.cpu.pc > 0x0805u) {
        printf("  FAIL: control was not transferred to 0800H\n");
        ok = 0;
    } else {
        printf("  OK: control is in the user program\n");
    }

    /* ПР */
    umk_interrupt(&m);
    umk_run_cycles(&m, 400000u);
    show_display(&m, "after ПР");

    /* РГ A */
    press(&m, KEY_RG);
    press(&m, KEY_DIGIT[0xA]);
    umk_display_clear_accumulator(&m);
    umk_run_cycles(&m, 200000u);
    show_display(&m, "РГ A");

    umk_display_pattern(&m, 25u, pat);
    printf("\n  expected              ");
    { unsigned i; for (i = 0; i < UMK_DIGITS; i++) printf(" %02X", EXPECTED[i]); }
    printf("   (\"  A - AA\")\n");

    if (memcmp(pat, EXPECTED, sizeof EXPECTED) != 0) {
        printf("\nFAIL: the display does not show \"A - AA\"\n");
        ok = 0;
    } else {
        printf("\nOK: the display shows \"A - AA\"\n");
    }

    printf("\n===== %s =====\n", ok ? "CRITERION 2 MET" : "CRITERION 2 NOT MET");
    return ok ? 0 : 1;
}
