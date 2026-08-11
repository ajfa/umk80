/* umkdis.c — desensamblador 8080 (entregable 4 del encargo).
 *
 * Uso:  umkdis <fichero.bin> [--org N] [--start N] [--end N] [--asm]
 *
 *   --org    dirección de carga de la imagen (por omisión 0)
 *   --start  primera dirección a desensamblar
 *   --end    última dirección (inclusive)
 *   --asm    salida reensamblable: sin las columnas de dirección y bytes
 *
 * Marca los diez opcodes no documentados del 8080 con un asterisco, para que
 * se distingan de un vistazo al leer código ajeno.
 */

#include "umk80/i8080.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const R8[8] = { "B","C","D","E","H","L","M","A" };
static const char *const RP[4] = { "B","D","H","SP" };
static const char *const RPS[4] = { "B","D","H","PSW" };
static const char *const ALU[8] = { "ADD","ADC","SUB","SBB","ANA","XRA","ORA","CMP" };
static const char *const ALUI[8] = { "ADI","ACI","SUI","SBI","ANI","XRI","ORI","CPI" };
static const char *const CC[8] = { "NZ","Z","NC","C","PO","PE","P","M" };

/* Escribe en `out` el texto de la instrucción que empieza en mem[pc].
 * Devuelve su longitud en bytes. */
static unsigned disasm(const unsigned char *mem, unsigned long pc,
                       unsigned long size, char *out, size_t outn)
{
    unsigned char op = mem[pc];
    unsigned len = i8080_opcode_length(op);
    unsigned char b1 = (pc + 1 < size) ? mem[pc + 1] : 0;
    unsigned char b2 = (pc + 2 < size) ? mem[pc + 2] : 0;
    unsigned imm8 = b1;
    unsigned imm16 = (unsigned)((b2 << 8) | b1);

    /* casos con nombre propio */
    switch (op) {
        case 0x00: snprintf(out, outn, "NOP"); return len;
        case 0x07: snprintf(out, outn, "RLC"); return len;
        case 0x0F: snprintf(out, outn, "RRC"); return len;
        case 0x17: snprintf(out, outn, "RAL"); return len;
        case 0x1F: snprintf(out, outn, "RAR"); return len;
        case 0x27: snprintf(out, outn, "DAA"); return len;
        case 0x2F: snprintf(out, outn, "CMA"); return len;
        case 0x37: snprintf(out, outn, "STC"); return len;
        case 0x3F: snprintf(out, outn, "CMC"); return len;
        case 0x76: snprintf(out, outn, "HLT"); return len;
        case 0xC9: snprintf(out, outn, "RET"); return len;
        case 0xD9: snprintf(out, outn, "RET"); return len;      /* no documentado */
        case 0xE3: snprintf(out, outn, "XTHL"); return len;
        case 0xE9: snprintf(out, outn, "PCHL"); return len;
        case 0xEB: snprintf(out, outn, "XCHG"); return len;
        case 0xF3: snprintf(out, outn, "DI"); return len;
        case 0xF9: snprintf(out, outn, "SPHL"); return len;
        case 0xFB: snprintf(out, outn, "EI"); return len;
        case 0x02: snprintf(out, outn, "STAX  B"); return len;
        case 0x12: snprintf(out, outn, "STAX  D"); return len;
        case 0x0A: snprintf(out, outn, "LDAX  B"); return len;
        case 0x1A: snprintf(out, outn, "LDAX  D"); return len;
        case 0x22: snprintf(out, outn, "SHLD  %04XH", imm16); return len;
        case 0x2A: snprintf(out, outn, "LHLD  %04XH", imm16); return len;
        case 0x32: snprintf(out, outn, "STA   %04XH", imm16); return len;
        case 0x3A: snprintf(out, outn, "LDA   %04XH", imm16); return len;
        case 0xC3: snprintf(out, outn, "JMP   %04XH", imm16); return len;
        case 0xCB: snprintf(out, outn, "JMP   %04XH", imm16); return len; /* no doc. */
        case 0xCD: case 0xDD: case 0xED: case 0xFD:
            snprintf(out, outn, "CALL  %04XH", imm16); return len;
        case 0xDB: snprintf(out, outn, "IN    %02XH", imm8); return len;
        case 0xD3: snprintf(out, outn, "OUT   %02XH", imm8); return len;
        case 0x08: case 0x10: case 0x18: case 0x20:
        case 0x28: case 0x30: case 0x38:
            snprintf(out, outn, "NOP"); return len;            /* no documentados */
        default: break;
    }

    if (op >= 0x40 && op <= 0x7F) {
        snprintf(out, outn, "MOV   %s,%s", R8[(op >> 3) & 7], R8[op & 7]);
    } else if (op >= 0x80 && op <= 0xBF) {
        snprintf(out, outn, "%-5s %s", ALU[(op >> 3) & 7], R8[op & 7]);
    } else if ((op & 0xCF) == 0x01) {
        snprintf(out, outn, "LXI   %s,%04XH", RP[(op >> 4) & 3], imm16);
    } else if ((op & 0xCF) == 0x03) {
        snprintf(out, outn, "INX   %s", RP[(op >> 4) & 3]);
    } else if ((op & 0xCF) == 0x0B) {
        snprintf(out, outn, "DCX   %s", RP[(op >> 4) & 3]);
    } else if ((op & 0xCF) == 0x09) {
        snprintf(out, outn, "DAD   %s", RP[(op >> 4) & 3]);
    } else if ((op & 0xCF) == 0xC5) {
        snprintf(out, outn, "PUSH  %s", RPS[(op >> 4) & 3]);
    } else if ((op & 0xCF) == 0xC1) {
        snprintf(out, outn, "POP   %s", RPS[(op >> 4) & 3]);
    } else if ((op & 0xC7) == 0x04) {
        snprintf(out, outn, "INR   %s", R8[(op >> 3) & 7]);
    } else if ((op & 0xC7) == 0x05) {
        snprintf(out, outn, "DCR   %s", R8[(op >> 3) & 7]);
    } else if ((op & 0xC7) == 0x06) {
        snprintf(out, outn, "MVI   %s,%02XH", R8[(op >> 3) & 7], imm8);
    } else if ((op & 0xC7) == 0xC6) {
        snprintf(out, outn, "%-5s %02XH", ALUI[(op >> 3) & 7], imm8);
    } else if ((op & 0xC7) == 0xC7) {
        snprintf(out, outn, "RST   %u", (unsigned)((op >> 3) & 7));
    } else if ((op & 0xC7) == 0xC2) {
        snprintf(out, outn, "J%-4s %04XH", CC[(op >> 3) & 7], imm16);
    } else if ((op & 0xC7) == 0xC4) {
        snprintf(out, outn, "C%-4s %04XH", CC[(op >> 3) & 7], imm16);
    } else if ((op & 0xC7) == 0xC0) {
        snprintf(out, outn, "R%s", CC[(op >> 3) & 7]);
    } else {
        snprintf(out, outn, "DB    %02XH", op);
        return 1;
    }
    return len;
}

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

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--org") == 0 && i + 1 < argc) org = strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc) { start = strtoul(argv[++i], NULL, 0); have_start = 1; }
        else if (strcmp(argv[i], "--end") == 0 && i + 1 < argc) { end = strtoul(argv[++i], NULL, 0); have_end = 1; }
        else if (strcmp(argv[i], "--asm") == 0) asm_only = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "uso: %s <fichero.bin> [--org N] [--start N] [--end N] [--asm]\n",
                argv[0]);
        return 2;
    }

    f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    n = fread(mem, 1, sizeof mem, f);
    fclose(f);

    if (!have_start) start = org;
    if (!have_end)   end = org + (unsigned long)n - 1;

    if (!asm_only) printf("; %s, %lu bytes, cargado en %04lXH\n\n",
                          path, (unsigned long)n, org);
    else           printf("      ORG   %04lXH\n\n", start);

    for (pc = start; pc <= end && pc - org < (unsigned long)n; ) {
        char txt[64];
        unsigned long off = pc - org;
        unsigned len = disasm(mem, off, (unsigned long)n, txt, sizeof txt);
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
                   i8080_opcode_undocumented(mem[off]) ? "  ; * no documentado" : "");
        }
        pc += len;
    }
    return 0;
}
