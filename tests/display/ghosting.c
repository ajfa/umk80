/* ghosting.c — acceptance criterion 3.
 *
 * Two versions of the same «HELLO» program, taken from the eax.me article.
 * The first does not blank the displays before changing the segment mask and
 * MUST produce ghosting: each digit shares its time between its own letter
 * and the previous digit's. The second blanks first and must show a clean
 * HELLO.
 *
 * The comparison is over the TIME-AVERAGED display state, not a snapshot, so
 * that it does not depend on when the sample is taken.
 */

#include "umk80/umk80.h"

#include <stdio.h>
#include <string.h>

/* Bootstrap: programs the КР580ВВ55А exactly as the monitor does
 * (МОН: MVI A, NOT CNTRWRD -> 89h) and jumps to the user program. */
static const uint8_t BOOTSTRAP[] = {
    0x3E, 0x89,             /* MVI A,89H     ; PA sal., PB sal., PC ent. */
    0xD3, 0xFB,             /* OUT 0FBH                                  */
    0xC3, 0x00, 0x08        /* JMP 0800H                                 */
};
#define BOOTSTRAP_ADDR 0x0980u

/* Naive version: OUT 0F8H selects the digit BEFORE setting its segments, so
 * during that interval the digit displays the previous digit's mask. */
static const uint8_t NAIVE[] = {
    0x06, 0x01,             /* 0800  MVI B,01H   */
    0x0E, 0x05,             /* 0802  MVI C,05H   */
    0x26, 0x08,             /* 0804  MVI H,08H   */
    0x2E, 0x18,             /* 0806  MVI L,18H   */
    0x78,                   /* 0808  MOV A,B     */
    0xD3, 0xF8,             /* 0809  OUT 0F8H    */
    0x07,                   /* 080B  RLC         */
    0x47,                   /* 080C  MOV B,A     */
    0x7E,                   /* 080D  MOV A,M     */
    0xD3, 0xF9,             /* 080E  OUT 0F9H    */
    0x23,                   /* 0810  INX H       */
    0x0D,                   /* 0811  DCR C       */
    0xC2, 0x08, 0x08,       /* 0812  JNZ 0808H   */
    0xC3, 0x00, 0x08,       /* 0815  JMP 0800H   */
    0x76, 0x79, 0x38, 0x38, 0x3F   /* 0818  'H','E','L','L','O' */
};

/* Corrected version: blanks all displays (SUB A / OUT 0F8H), sets the
 * segments, and only then selects the digit. */
static const uint8_t FIXED[] = {
    0x06, 0x01,             /* 0800  MVI B,01H   */
    0x0E, 0x05,             /* 0802  MVI C,05H   */
    0x26, 0x08,             /* 0804  MVI H,08H   */
    0x2E, 0x1B,             /* 0806  MVI L,1BH   */
    0x97,                   /* 0808  SUB A       */
    0xD3, 0xF8,             /* 0809  OUT 0F8H    */
    0x7E,                   /* 080B  MOV A,M     */
    0xD3, 0xF9,             /* 080C  OUT 0F9H    */
    0x78,                   /* 080E  MOV A,B     */
    0xD3, 0xF8,             /* 080F  OUT 0F8H    */
    0x07,                   /* 0811  RLC         */
    0x47,                   /* 0812  MOV B,A     */
    0x23,                   /* 0813  INX H       */
    0x0D,                   /* 0814  DCR C       */
    0xC2, 0x08, 0x08,       /* 0815  JNZ 0808H   */
    0xC3, 0x00, 0x08,       /* 0818  JMP 0800H   */
    0x76, 0x79, 0x38, 0x38, 0x3F   /* 081B  'H','E','L','L','O' */
};

/* 'H','E','L','L','O' with the sixth display dark. The program starts with
 * B = 01h and rotates left, so the letters land on PORTA bits 0..4, which are
 * the five leftmost displays (UNKNOWNS §3). */
static const uint8_t HELLO[UMK_DIGITS] = { 0x76, 0x79, 0x38, 0x38, 0x3F, 0x00 };

static const char *SEGNAME = "ABCDEFG.";

static void print_pattern(const char *label, const uint8_t p[UMK_DIGITS])
{
    unsigned i, s;
    printf("  %-10s", label);
    for (i = 0; i < UMK_DIGITS; i++) printf(" %02X", p[i]);
    printf("   segments:");
    for (i = 0; i < UMK_DIGITS; i++) {
        printf(" [");
        for (s = 0; s < 7u; s++) putchar((p[i] & (1u << s)) ? SEGNAME[s] : '-');
        printf("]");
    }
    putchar('\n');
}

static void print_relative(umk_machine_t *m)
{
    uint8_t rel[UMK_DIGITS][UMK_SEGMENTS];
    unsigned i, s;
    umk_display_relative(m, rel);
    for (i = 0; i < UMK_DIGITS; i++) {
        printf("    digit %u (%s):", i, i == 0 ? "left" : (i == 5 ? "right" : "    "));
        for (s = 0; s < 7u; s++) printf(" %c=%3u", SEGNAME[s], rel[i][s]);
        putchar('\n');
    }
}

static void run_program(umk_machine_t *m, const uint8_t *prog, size_t len)
{
    umk_init(m, UMK_REV2);
    umk_load_ram(m, 0x0800u, prog, len);
    umk_load_ram(m, BOOTSTRAP_ADDR, BOOTSTRAP, sizeof BOOTSTRAP);
    m->cpu.pc = BOOTSTRAP_ADDR;

    /* A long warm-up so it settles, then a clean window of simulated time to
     * average over. */
    umk_run_cycles(m, 200000u);
    umk_display_clear_accumulator(m);
    umk_run_cycles(m, 200000u);
}

int main(void)
{
    static umk_machine_t m;
    uint8_t naive_pat[UMK_DIGITS], fixed_pat[UMK_DIGITS];
    int ok = 1;

    /* Low threshold: it also picks up the intruding ghost letter, which in
     * the naive version occupies about 40 % of each digit's time. */
    const uint8_t THRESHOLD = 25u;

    printf("=== Criterion 3: multiplexing fidelity ===\n\n");
    printf("Expected pattern for HELLO:\n");
    print_pattern("HELLO", HELLO);

    printf("\n-- naive version (does not blank before changing segments) --\n");
    run_program(&m, NAIVE, sizeof NAIVE);
    umk_display_pattern(&m, THRESHOLD, naive_pat);
    print_pattern("naive", naive_pat);
    print_relative(&m);

    printf("\n-- corrected version (blank, set segments, then select) --\n");
    run_program(&m, FIXED, sizeof FIXED);
    umk_display_pattern(&m, THRESHOLD, fixed_pat);
    print_pattern("corrected", fixed_pat);
    print_relative(&m);

    printf("\n=== verdict ===\n");

    if (memcmp(naive_pat, HELLO, sizeof HELLO) == 0) {
        printf("FAIL: the naive version matches HELLO; there is no ghosting.\n");
        ok = 0;
    } else {
        printf("OK: the naive version DIFFERS from HELLO (ghosting present).\n");
    }

    if (memcmp(fixed_pat, HELLO, sizeof HELLO) != 0) {
        printf("FAIL: the corrected version does NOT match HELLO.\n");
        ok = 0;
    } else {
        printf("OK: the corrected version matches HELLO exactly.\n");
    }

    printf("\n===== %s =====\n", ok ? "CRITERION 3 MET" : "CRITERION 3 NOT MET");
    return ok ? 0 : 1;
}
