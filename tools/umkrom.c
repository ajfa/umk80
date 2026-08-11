/* umkrom.c — reconstruye la imagen de ПЗУ a partir de la columna OBJ del
 * listado transcrito (rom/monitor.lst).
 *
 * Es la primera de las dos vías de PLAN.md §4. La segunda es reensamblar la
 * columna de fuente con umkasm; las dos imágenes tienen que coincidir byte a
 * byte.
 *
 * Uso:  umkrom rom/monitor.lst rom/monitor.bin
 *
 * Además del binario imprime un informe de cobertura: qué tramos quedan sin
 * cubrir y si alguna dirección recibe dos valores distintos. Un hueco dentro
 * del cuerpo del monitor es señal de que falta transcribir algo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "console.h"

#define ROM_SIZE 2048u

static unsigned char image[ROM_SIZE];
static unsigned char covered[ROM_SIZE];
static int errors;

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int is_hex_string(const char *s, size_t n)
{
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) if (hexval((unsigned char)s[i]) < 0) return 0;
    return 1;
}

static void place(unsigned addr, unsigned char val, unsigned lineno)
{
    if (addr >= ROM_SIZE) {
        fprintf(stderr, "línea %u: dirección %04X fuera del ПЗУ\n", lineno, addr);
        errors++;
        return;
    }
    if (covered[addr] && image[addr] != val) {
        fprintf(stderr, "línea %u: %04X recibe %02X pero ya valía %02X\n",
                lineno, addr, val, image[addr]);
        errors++;
        return;
    }
    image[addr] = val;
    covered[addr] = 1;
}

int main(int argc, char **argv)
{
    FILE *in, *out, *asmf = NULL;
    char line[512];
    unsigned lineno = 0, total = 0;
    unsigned i, run_start;
    int in_gap, a;
    const char *asmpath = NULL;

    console_utf8();

    

    for (a = 3; a < argc; a++)
        if (strcmp(argv[a], "--asm") == 0 && a + 1 < argc) asmpath = argv[++a];

    if (argc < 3) {
        fprintf(stderr, "uso: %s <monitor.lst> <monitor.bin> [--asm <monitor.asm>]\n",
                argv[0]);
        return 2;
    }

    memset(image, 0xFF, sizeof image);   /* EPROM virgen */

    in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 2; }

    if (asmpath) {
        asmf = fopen(asmpath, "wb");
        if (!asmf) { perror(asmpath); return 2; }
        fprintf(asmf, "; Fuente del monitor del УМК-80, extraído de rom/monitor.lst.\n"
                      "; NO EDITAR: se regenera con  umkrom rom/monitor.lst ... --asm\n"
                      "; Original: Р.Р.00004-01 12 01-1, «Системный монитор. Текст\n"
                      "; программы», 1986, литера О1, ISIS-II 8080/8085 MACRO ASSEMBLER.\n\n");
    }

    while (fgets(line, sizeof line, in)) {
        char *bar, *p, *loc_tok, *obj_tok;
        unsigned addr;
        size_t objlen, k;

        lineno++;
        if (line[0] == '#') continue;

        bar = strchr(line, '|');
        if (!bar) continue;

        if (asmf) {                       /* la columna de fuente, tal cual */
            char *srctext = bar + 1;
            char *e = srctext + strlen(srctext);
            while (e > srctext && (e[-1] == '\n' || e[-1] == '\r')) e--;
            *e = '\0';
            fprintf(asmf, "%s\n", srctext);
        }

        *bar = '\0';                      /* nos quedamos con LOC y OBJ */

        p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) continue;

        loc_tok = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';

        if (strlen(loc_tok) != 4 || !is_hex_string(loc_tok, 4)) {
            fprintf(stderr, "línea %u: LOC ilegible «%s»\n", lineno, loc_tok);
            errors++;
            continue;
        }
        addr = (unsigned)strtoul(loc_tok, NULL, 16);

        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) continue;                /* LOC sin OBJ: ORG, EQU, etiqueta */

        obj_tok = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        *p = '\0';

        objlen = strlen(obj_tok);
        if (!is_hex_string(obj_tok, objlen) || (objlen % 2u) != 0u) {
            fprintf(stderr, "línea %u: OBJ ilegible «%s»\n", lineno, obj_tok);
            errors++;
            continue;
        }

        for (k = 0; k < objlen; k += 2) {
            int hi = hexval((unsigned char)obj_tok[k]);
            int lo = hexval((unsigned char)obj_tok[k + 1]);
            place(addr + (unsigned)(k / 2), (unsigned char)((hi << 4) | lo), lineno);
            total++;
        }
    }
    fclose(in);
    if (asmf) { fclose(asmf); printf("%s escrito\n", asmpath); }

    /* Informe de cobertura */
    printf("bytes colocados: %u\n", total);
    printf("tramos sin cubrir dentro de 0000-07FF:\n");
    in_gap = 0; run_start = 0;
    for (i = 0; i <= ROM_SIZE; i++) {
        int gap = (i < ROM_SIZE) && !covered[i];
        if (gap && !in_gap) { in_gap = 1; run_start = i; }
        else if (!gap && in_gap) {
            in_gap = 0;
            printf("  %04X-%04X  (%u bytes)\n", run_start, i - 1, i - run_start);
        }
    }

    out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 2; }
    if (fwrite(image, 1, ROM_SIZE, out) != ROM_SIZE) { perror("write"); return 2; }
    fclose(out);

    printf("%s escrito (%u bytes)\n", argv[2], ROM_SIZE);
    if (errors) printf("HAY %d PROBLEMAS DE TRANSCRIPCIÓN\n", errors);
    return errors ? 1 : 0;
}


