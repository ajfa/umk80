/* cpu_suite.c — arnés CP/M mínimo para las suites de validación del 8080.
 *
 * Criterio de aceptación 1 del encargo: TST8080, 8080PRE, CPUTEST y 8080EXM.
 *
 * Las cuatro suites son programas CP/M (.COM) que se cargan en 0x0100 y usan
 * dos servicios del BDOS: la función 2 (imprimir el carácter que hay en E) y
 * la función 9 (imprimir la cadena terminada en '$' que apunta DE). En vez de
 * interceptar el PC se parchean dos puntos de la memoria baja, que es lo que
 * hace el propio CP/M:
 *
 *   0x0000:  OUT 0    -> el programa terminó (salto a WBOOT)
 *   0x0005:  IN 0     -> llamada al BDOS
 *            RET
 *
 * Esto no forma parte del núcleo: es una herramienta de prueba y sí usa la
 * biblioteca estándar.
 */

#include "umk80/i8080.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 0x10000u
#define OUT_MAX  (1u << 20)

typedef struct {
    uint8_t  mem[MEM_SIZE];
    char     out[OUT_MAX];
    size_t   out_len;
    int      finished;
    i8080_t *cpu;
} host_t;

/* CPUTEST emite algún byte NUL entre el texto. Se imprime tal cual, pero no
 * se guarda en el búfer de captura: un NUL en medio truncaría el strstr con
 * el que se comprueba el resultado. */
static void emit(host_t *h, char ch)
{
    if (ch != '\0' && h->out_len + 1u < OUT_MAX) h->out[h->out_len++] = ch;
    fputc(ch, stdout);
}

static uint8_t host_read(void *ud, uint16_t addr, uint8_t status)
{
    (void)status;
    return ((host_t *)ud)->mem[addr];
}

static void host_write(void *ud, uint16_t addr, uint8_t val, uint8_t status)
{
    (void)status;
    ((host_t *)ud)->mem[addr] = val;
}

static uint8_t host_in(void *ud, uint8_t port)
{
    host_t *h = (host_t *)ud;
    i8080_t *c = h->cpu;

    if (port == 0u) {                      /* llamada al BDOS */
        if (c->c == 9u) {                  /* imprimir cadena terminada en '$' */
            uint16_t p = (uint16_t)((c->d << 8) | c->e);
            unsigned guard = 0;
            while (h->mem[p] != '$' && guard++ < MEM_SIZE) {
                emit(h, (char)h->mem[p]);
                p = (uint16_t)(p + 1u);
            }
        } else if (c->c == 2u) {           /* imprimir un carácter */
            emit(h, (char)c->e);
        }
    }
    return 0u;
}

static void host_out(void *ud, uint8_t port, uint8_t val)
{
    (void)val;
    if (port == 0u) ((host_t *)ud)->finished = 1;
}

/* ------------------------------------------------------------------------- */

typedef struct {
    const char *file;
    const char *expect;      /* debe aparecer en la salida */
    const char *forbid;      /* no debe aparecer (NULL = sin restricción) */
    uint64_t    max_cycles;
} suite_t;

static int run_suite(const char *dir, const suite_t *s)
{
    static host_t h;                       /* 64 KB + búfer: fuera de la pila */
    i8080_t cpu;
    i8080_bus_t bus;
    char path[512];
    FILE *f;
    size_t n;
    int ok;

    memset(&h, 0, sizeof h);
    h.cpu = &cpu;

    snprintf(path, sizeof path, "%s/%s", dir, s->file);
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "no se pudo abrir %s\n", path);
        return 0;
    }
    n = fread(h.mem + 0x0100, 1u, MEM_SIZE - 0x0100u, f);
    fclose(f);

    /* Parches de la memoria baja de CP/M. */
    h.mem[0x0000] = 0xD3; h.mem[0x0001] = 0x00;                 /* OUT 0  */
    h.mem[0x0005] = 0xDB; h.mem[0x0006] = 0x00; h.mem[0x0007] = 0xC9; /* IN 0 / RET */

    bus.read = host_read; bus.write = host_write;
    bus.in = host_in;     bus.out = host_out;
    bus.ud = &h;

    i8080_reset(&cpu);
    cpu.pc = 0x0100;

    printf("\n=== %s (%lu bytes) ===\n", s->file, (unsigned long)n);

    while (!h.finished && cpu.cycles < s->max_cycles) {
        if (cpu.halted) {
            printf("\n[HLT en 0x%04X — la suite aborta]\n", cpu.pc);
            break;
        }
        i8080_step(&cpu, &bus);
    }

    h.out[h.out_len] = '\0';
    printf("\n--- %llu ciclos T ---\n", (unsigned long long)cpu.cycles);

    ok = 1;
    if (s->expect && !strstr(h.out, s->expect)) {
        printf("FALLO: no aparece \"%s\"\n", s->expect);
        ok = 0;
    }
    if (s->forbid && strstr(h.out, s->forbid)) {
        printf("FALLO: aparece \"%s\"\n", s->forbid);
        ok = 0;
    }
    if (!h.finished && cpu.cycles >= s->max_cycles) {
        printf("FALLO: se agotó el presupuesto de ciclos\n");
        ok = 0;
    }
    printf("%s: %s\n", s->file, ok ? "OK" : "FALLO");
    return ok;
}

int main(int argc, char **argv)
{
    static const suite_t suites[] = {
        { "TST8080.COM", "CPU IS OPERATIONAL",             NULL,    100000000ull },
        { "8080PRE.COM", "8080 Preliminary tests complete", NULL,   100000000ull },
        { "CPUTEST.COM", "CPU TESTS OK",                   NULL,   1000000000ull },
        { "8080EXM.COM", "Tests complete",                 "ERROR", 60000000000ull },
    };
    const char *dir = (argc > 1) ? argv[1] : "suites";
    int only_quick = (argc > 2 && strcmp(argv[2], "--quick") == 0);
    size_t i, count = sizeof suites / sizeof suites[0];
    int all_ok = 1;

    if (only_quick) count = 3;   /* 8080EXM aparte: tarda minutos */

    for (i = 0; i < count; i++) {
        if (!run_suite(dir, &suites[i])) all_ok = 0;
    }

    printf("\n===== %s =====\n", all_ok ? "TODAS LAS SUITES PASAN" : "HAY FALLOS");
    return all_ok ? 0 : 1;
}
