/* umkdis.c — 8080 disassembler.
 *
 * Usage:  umkdis <file.bin> [--org N] [--start N] [--end N] [--asm]
 *
 *   --org    load address of the image (default 0)
 *   --start  first address to disassemble
 *   --end    last address (inclusive)
 *   --asm    reassemblable output: no address and byte columns
 *
 * The decoding lives in disasm.c, shared with the umkcli debugger. The ten
 * undocumented opcodes are marked with an asterisk so they stand out at a
 * glance when reading someone else's code.
 */

#include "disasm.h"
#include "umk80/i8080.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"

int main(int argc, char **argv)
{
    static unsigned char mem[65536];
    const char *path = NULL;
    unsigned long org = 0, start = 0, end = 0;
    int have_start = 0, have_end = 0, asm_only = 0;
    unsigned long pc;
    size_t n;
    FILE *f;
    int i;

    console_utf8();

    

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--org") == 0 && i + 1 < argc)
            org = strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc) {
            start = strtoul(argv[++i], NULL, 0); have_start = 1;
        } else if (strcmp(argv[i], "--end") == 0 && i + 1 < argc) {
            end = strtoul(argv[++i], NULL, 0); have_end = 1;
        } else if (strcmp(argv[i], "--asm") == 0) asm_only = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: %s <file.bin> [--org N] [--start N] [--end N] [--asm]\n",
                argv[0]);
        return 2;
    }

    f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    n = fread(mem, 1, sizeof mem, f);
    fclose(f);

    if (!have_start) start = org;
    if (!have_end)   end = org + (unsigned long)n - 1;

    if (!asm_only) printf("; %s, %lu bytes, loaded at %04lXH\n\n",
                          path, (unsigned long)n, org);
    else           printf("      ORG   %04lXH\n\n", start);

    for (pc = start; pc <= end && pc - org < (unsigned long)n; ) {
        char txt[64];
        unsigned long off = pc - org;
        unsigned len = umk_disasm(mem, off, (unsigned long)n, txt, sizeof txt);
        unsigned k;

        if (asm_only) {
            printf("      %s\n", txt);
        } else {
            printf("%04lX  ", pc);
            for (k = 0; k < 3u; k++) {
                if (k < len) printf("%02X ", mem[off + k]);
                else printf("   ");
            }
            printf(" %-22s%s\n", txt,
                   i8080_opcode_undocumented(mem[off]) ? "  ; * undocumented" : "");
        }
        pc += len;
    }
    return 0;
}


