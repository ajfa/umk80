/* ghosting.c — criterio de aceptación 3 del encargo.
 *
 * Dos versiones del mismo programa «HELLO», tomadas literalmente del
 * encargo (y del artículo de eax.me). La primera no apaga los indicadores
 * antes de cambiar la máscara de segmentos y TIENE que producir fantasmeo:
 * cada dígito comparte su tiempo entre su letra y la del dígito anterior.
 * La segunda apaga primero y tiene que mostrar HELLO limpio.
 *
 * La comparación es sobre el estado PROMEDIADO del display, no sobre una
 * instantánea, para que no dependa del momento del muestreo.
 */

#include "umk80/umk80.h"

#include <stdio.h>
#include <string.h>

/* Arranque: programa el КР580ВВ55А igual que hace el monitor
 * (МОН: MVI A, NOT CNTRWRD -> 89h) y salta al programa de usuario. */
static const uint8_t BOOTSTRAP[] = {
    0x3E, 0x89,             /* MVI A,89H     ; PA sal., PB sal., PC ent. */
    0xD3, 0xFB,             /* OUT 0FBH                                  */
    0xC3, 0x00, 0x08        /* JMP 0800H                                 */
};
#define BOOTSTRAP_ADDR 0x0980u

/* Versión ingenua: OUT 0F8H selecciona el dígito ANTES de fijar sus
 * segmentos, de modo que durante ese intervalo el dígito luce la máscara
 * del dígito anterior. */
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

/* Versión corregida: apaga todos los indicadores (SUB A / OUT 0F8H), fija
 * los segmentos y sólo entonces selecciona el dígito. */
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

/* 'H','E','L','L','O' y el sexto indicador apagado. El programa arranca con
 * B = 01h y rota a la izquierda, luego las letras caen en los bits 0..4 de
 * PORTA, que son los cinco indicadores de la izquierda (DESCONOCIDOS §3). */
static const uint8_t HELLO[UMK_DIGITS] = { 0x76, 0x79, 0x38, 0x38, 0x3F, 0x00 };

static const char *SEGNAME = "ABCDEFG.";

static void print_pattern(const char *label, const uint8_t p[UMK_DIGITS])
{
    unsigned i, s;
    printf("  %-10s", label);
    for (i = 0; i < UMK_DIGITS; i++) printf(" %02X", p[i]);
    printf("   segmentos:");
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
        printf("    dígito %u (%s):", i, i == 0 ? "izq" : (i == 5 ? "der" : "   "));
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

    /* Un arranque largo para que se estabilice, y luego una ventana limpia
     * de 100 ms de tiempo simulado sobre la que promediar. */
    umk_run_cycles(m, 200000u);
    umk_display_clear_accumulator(m);
    umk_run_cycles(m, 200000u);
}

int main(void)
{
    static umk_machine_t m;
    uint8_t naive_pat[UMK_DIGITS], fixed_pat[UMK_DIGITS];
    int ok = 1;

    /* Umbral bajo: recoge también la letra intrusa del fantasmeo, que en la
     * versión ingenua ocupa alrededor del 40 % del tiempo de cada dígito. */
    const uint8_t THRESHOLD = 25u;

    printf("=== Criterio 3: fidelidad del multiplexado ===\n\n");
    printf("Patrón esperado para HELLO:\n");
    print_pattern("HELLO", HELLO);

    printf("\n-- versión ingenua (no apaga antes de cambiar segmentos) --\n");
    run_program(&m, NAIVE, sizeof NAIVE);
    umk_display_pattern(&m, THRESHOLD, naive_pat);
    print_pattern("ingenua", naive_pat);
    print_relative(&m);

    printf("\n-- versión corregida (apaga, fija segmentos, selecciona) --\n");
    run_program(&m, FIXED, sizeof FIXED);
    umk_display_pattern(&m, THRESHOLD, fixed_pat);
    print_pattern("corregida", fixed_pat);
    print_relative(&m);

    printf("\n=== veredicto ===\n");

    if (memcmp(naive_pat, HELLO, sizeof HELLO) == 0) {
        printf("FALLO: la versión ingenua coincide con HELLO; no hay fantasmeo.\n");
        ok = 0;
    } else {
        printf("OK: la versión ingenua DIFIERE de HELLO (fantasmeo presente).\n");
    }

    if (memcmp(fixed_pat, HELLO, sizeof HELLO) != 0) {
        printf("FALLO: la versión corregida NO coincide con HELLO.\n");
        ok = 0;
    } else {
        printf("OK: la versión corregida coincide exactamente con HELLO.\n");
    }

    printf("\n===== %s =====\n", ok ? "CRITERIO 3 CUMPLIDO" : "CRITERIO 3 NO CUMPLIDO");
    return ok ? 0 : 1;
}
