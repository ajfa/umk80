/* umkasm.c — 8080 assembler.
 *
 * Accepts ISIS-II 8080/8085 MACRO ASSEMBLER syntax to the extent the УМК-80
 * monitor listing uses it: labels, ORG, EQU, DB, DW, DS, END, expressions
 * with + and -, the NOT operator, the location counter $, hexadecimal
 * (0F8H), binary (00100000B) and decimal constants, and character literals
 * ('A').
 *
 * Usage:  umkasm <source.asm> <output.bin> [--size N] [--verify <ref.bin>]
 *
 * This is the second path of PLAN.md §4: reassembling the listing's text must
 * produce exactly the same image as rebuilding the OBJ column with umkrom. If
 * the two disagree, there is a transcription error.
 *
 * Passes are repeated until the symbol table stops changing, because the
 * monitor source contains forward-referencing EQUs (PLLOC EQU PCLOC+1, with
 * the label PCLOC defined 900 lines further down).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "console.h"

#define MAX_LINES 6000
#define MAX_LINE   512
#define MAX_SYMS   1024
#define IMAGE_MAX  65536

static char  lines[MAX_LINES][MAX_LINE];
static int   nlines;

static struct { char name[32]; long value; int defined; } syms[MAX_SYMS];
static int nsyms;

static unsigned char image[IMAGE_MAX];
static unsigned char covered[IMAGE_MAX];
static long image_size = 2048;

static int errors, cur_line;
static long pc, stmt_pc;

static void err(const char *fmt, const char *a)
{
    fprintf(stderr, "línea %d: ", cur_line + 1);
    fprintf(stderr, fmt, a);
    fputc('\n', stderr);
    errors++;
}

/* --- symbol table ------------------------------------------------------ */

static int sym_find(const char *name)
{
    int i;
    for (i = 0; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0) return i;
    return -1;
}

/* Returns 1 if the value changed from the previous pass. */
static int sym_set(const char *name, long value)
{
    int i = sym_find(name);
    if (i < 0) {
        if (nsyms >= MAX_SYMS) { err("too many symbols (%s)", name); return 0; }
        i = nsyms++;
        strncpy(syms[i].name, name, sizeof syms[i].name - 1);
        syms[i].name[sizeof syms[i].name - 1] = '\0';
        syms[i].value = value;
        syms[i].defined = 1;
        return 1;
    }
    if (syms[i].value != value) { syms[i].value = value; return 1; }
    return 0;
}

/* --- expression evaluator ---------------------------------------------------
 *
 * Grammar: expr := term { ('+' | '-') term }
 *          term := ['NOT'] factor
 *          factor := number | character | symbol | '$' | '(' expr ')'
 */

static const char *ep;
static int eval_ok;

static void eskip(void) { while (*ep == ' ' || *ep == '\t') ep++; }

static long eval_expr(void);

static long eval_factor(void)
{
    eskip();
    if (*ep == '$') { ep++; return stmt_pc; }
    if (*ep == '(') {
        long v;
        ep++;
        v = eval_expr();
        eskip();
        if (*ep == ')') ep++;
        return v;
    }
    if (*ep == '\'') {                    /* character literal */
        long v;
        ep++;
        v = (unsigned char)*ep++;
        if (*ep == '\'') ep++;
        return v;
    }
    if (isdigit((unsigned char)*ep)) {
        const char *s = ep;
        char buf[64];
        size_t n = 0;
        while (isalnum((unsigned char)*ep)) {
            if (n < sizeof buf - 1) buf[n++] = *ep;
            ep++;
        }
        buf[n] = '\0';
        (void)s;
        if (n > 1 && (buf[n-1] == 'H' || buf[n-1] == 'h')) {
            buf[n-1] = '\0';
            return strtol(buf, NULL, 16);
        }
        if (n > 1 && (buf[n-1] == 'B' || buf[n-1] == 'b')) {
            size_t k; int bin = 1;
            for (k = 0; k + 1 < n; k++) if (buf[k] != '0' && buf[k] != '1') bin = 0;
            if (bin) { buf[n-1] = '\0'; return strtol(buf, NULL, 2); }
        }
        if (n > 1 && (buf[n-1] == 'D' || buf[n-1] == 'd')) { buf[n-1] = '\0'; }
        return strtol(buf, NULL, 10);
    }
    if (isalpha((unsigned char)*ep) || *ep == '_' || *ep == '?') {
        char name[32];
        size_t n = 0;
        int i;
        while (isalnum((unsigned char)*ep) || *ep == '_' || *ep == '?') {
            if (n < sizeof name - 1) name[n++] = (char)toupper((unsigned char)*ep);
            ep++;
        }
        name[n] = '\0';
        i = sym_find(name);
        if (i < 0) { eval_ok = 0; return 0; }
        return syms[i].value;
    }
    eval_ok = 0;
    return 0;
}

static long eval_term(void)
{
    eskip();
    if (strncmp(ep, "NOT", 3) == 0 && !isalnum((unsigned char)ep[3])) {
        ep += 3;
        return ~eval_term();
    }
    if (*ep == '-') { ep++; return -eval_term(); }
    if (*ep == '+') { ep++; return eval_term(); }
    return eval_factor();
}

static long eval_expr(void)
{
    long v = eval_term();
    for (;;) {
        eskip();
        if (*ep == '+') { ep++; v += eval_term(); }
        else if (*ep == '-') { ep++; v -= eval_term(); }
        else break;
    }
    return v;
}

static long evaluate(const char *s, int *ok)
{
    long v;
    ep = s;
    eval_ok = 1;
    v = eval_expr();
    *ok = eval_ok;
    return v;
}

/* --- instruction table -------------------------------------------------- */

typedef enum {
    K_NONE,      /* no operands                                  */
    K_R8D,       /* INR/DCR r      : base + (r<<3)                */
    K_R8S,       /* ALU r          : base + r                     */
    K_MOV,       /* MOV d,s        : 40h + (d<<3) + s             */
    K_MVI,       /* MVI r,imm8     : base + (r<<3), imm           */
    K_LXI,       /* LXI rp,imm16                                  */
    K_RP,        /* INX/DCX/DAD rp : base + (rp<<4)               */
    K_STK,       /* PUSH/POP rp    : base + (rp<<4), with PSW     */
    K_AX,        /* STAX/LDAX      : B and D only                 */
    K_IMM8,      /* ADI, CPI...                                   */
    K_ADDR,      /* JMP, CALL, LDA...                             */
    K_RST,       /* RST n                                         */
    K_PORT       /* IN, OUT                                       */
} kind_t;

typedef struct { const char *name; unsigned char base; kind_t kind; } mnem_t;

static const mnem_t MNEM[] = {
    {"NOP",0x00,K_NONE},{"RLC",0x07,K_NONE},{"RRC",0x0F,K_NONE},
    {"RAL",0x17,K_NONE},{"RAR",0x1F,K_NONE},{"DAA",0x27,K_NONE},
    {"CMA",0x2F,K_NONE},{"STC",0x37,K_NONE},{"CMC",0x3F,K_NONE},
    {"HLT",0x76,K_NONE},{"RET",0xC9,K_NONE},{"XTHL",0xE3,K_NONE},
    {"PCHL",0xE9,K_NONE},{"XCHG",0xEB,K_NONE},{"DI",0xF3,K_NONE},
    {"SPHL",0xF9,K_NONE},{"EI",0xFB,K_NONE},
    {"RNZ",0xC0,K_NONE},{"RZ",0xC8,K_NONE},{"RNC",0xD0,K_NONE},
    {"RC",0xD8,K_NONE},{"RPO",0xE0,K_NONE},{"RPE",0xE8,K_NONE},
    {"RP",0xF0,K_NONE},{"RM",0xF8,K_NONE},

    {"INR",0x04,K_R8D},{"DCR",0x05,K_R8D},

    {"ADD",0x80,K_R8S},{"ADC",0x88,K_R8S},{"SUB",0x90,K_R8S},
    {"SBB",0x98,K_R8S},{"ANA",0xA0,K_R8S},{"XRA",0xA8,K_R8S},
    {"ORA",0xB0,K_R8S},{"CMP",0xB8,K_R8S},

    {"MOV",0x40,K_MOV},{"MVI",0x06,K_MVI},{"LXI",0x01,K_LXI},

    {"INX",0x03,K_RP},{"DCX",0x0B,K_RP},{"DAD",0x09,K_RP},
    {"PUSH",0xC5,K_STK},{"POP",0xC1,K_STK},
    {"STAX",0x02,K_AX},{"LDAX",0x0A,K_AX},

    {"ADI",0xC6,K_IMM8},{"ACI",0xCE,K_IMM8},{"SUI",0xD6,K_IMM8},
    {"SBI",0xDE,K_IMM8},{"ANI",0xE6,K_IMM8},{"XRI",0xEE,K_IMM8},
    {"ORI",0xF6,K_IMM8},{"CPI",0xFE,K_IMM8},

    {"JMP",0xC3,K_ADDR},{"JNZ",0xC2,K_ADDR},{"JZ",0xCA,K_ADDR},
    {"JNC",0xD2,K_ADDR},{"JC",0xDA,K_ADDR},{"JPO",0xE2,K_ADDR},
    {"JPE",0xEA,K_ADDR},{"JP",0xF2,K_ADDR},{"JM",0xFA,K_ADDR},
    {"CALL",0xCD,K_ADDR},{"CNZ",0xC4,K_ADDR},{"CZ",0xCC,K_ADDR},
    {"CNC",0xD4,K_ADDR},{"CC",0xDC,K_ADDR},{"CPO",0xE4,K_ADDR},
    {"CPE",0xEC,K_ADDR},{"CP",0xF4,K_ADDR},{"CM",0xFC,K_ADDR},
    {"LDA",0x3A,K_ADDR},{"STA",0x32,K_ADDR},
    {"LHLD",0x2A,K_ADDR},{"SHLD",0x22,K_ADDR},

    {"RST",0xC7,K_RST},
    {"IN",0xDB,K_PORT},{"OUT",0xD3,K_PORT}
};
#define MNEM_N ((int)(sizeof MNEM / sizeof MNEM[0]))

static int reg8(const char *s)
{
    if (!s || s[1] != '\0') return -1;
    switch (toupper((unsigned char)s[0])) {
        case 'B': return 0; case 'C': return 1; case 'D': return 2;
        case 'E': return 3; case 'H': return 4; case 'L': return 5;
        case 'M': return 6; case 'A': return 7;
        default:  return -1;
    }
}

static int regpair(const char *s, int stack)
{
    if (!s) return -1;
    if (strcmp(s, "B") == 0) return 0;
    if (strcmp(s, "D") == 0) return 1;
    if (strcmp(s, "H") == 0) return 2;
    if (!stack && strcmp(s, "SP") == 0) return 3;
    if (stack && strcmp(s, "PSW") == 0) return 3;
    return -1;
}

/* --- emission ------------------------------------------------------------------ */

static int emitting;

static void emit(long addr, unsigned char v)
{
    if (addr < 0 || addr >= image_size) {
        if (emitting) {
            char b[32];
            sprintf(b, "%04lXH", addr);
            err("address %s outside the image", b);
        }
        return;
    }
    if (emitting) { image[addr] = v; covered[addr] = 1; }
}

static void emit_byte(unsigned char v)  { emit(pc, v); pc++; }
static void emit_word(long v)
{
    emit_byte((unsigned char)(v & 0xFF));
    emit_byte((unsigned char)((v >> 8) & 0xFF));
}

/* --- parsing one line ---------------------------------------------------- */

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) e--;
    *e = '\0';
    return s;
}

/* Splits the operand list on top-level commas (outside quotes). */
static int split_operands(char *s, char *out[], int max)
{
    int n = 0;
    int inq = 0;
    char *start = s;
    for (;;) {
        if (*s == '\'') inq = !inq;
        if ((*s == ',' && !inq) || *s == '\0') {
            int last = (*s == '\0');
            *s = '\0';
            if (n < max) out[n++] = trim(start);
            if (last) break;
            start = s + 1;
        }
        s++;
    }
    if (n == 1 && out[0][0] == '\0') n = 0;
    return n;
}

static int changed;

static void assemble_line(char *raw)
{
    char work[MAX_LINE];
    char *s, *mnemonic, *rest;
    char *ops[8];
    int nops, i, ok = 1;

    strncpy(work, raw, sizeof work - 1);
    work[sizeof work - 1] = '\0';

    /* strip the comment, respecting character literals */
    {
        int inq = 0;
        char *p;
        for (p = work; *p; p++) {
            if (*p == '\'') inq = !inq;
            else if (*p == ';' && !inq) { *p = '\0'; break; }
        }
    }

    s = trim(work);
    if (!*s) return;
    if (*s == '$') return;             /* $EJECT and similar ISIS-II directives */

    stmt_pc = pc;

    /* label with a colon */
    {
        char *colon = strchr(s, ':');
        if (colon) {
            char name[32];
            size_t n = (size_t)(colon - s);
            if (n >= sizeof name) n = sizeof name - 1;
            for (i = 0; i < (int)n; i++) name[i] = (char)toupper((unsigned char)s[i]);
            name[n] = '\0';
            if (sym_set(name, pc)) changed = 1;
            s = trim(colon + 1);
            if (!*s) return;
        }
    }

    /* mnemonic, or the name of an EQU */
    mnemonic = s;
    while (*s && !isspace((unsigned char)*s)) s++;
    if (*s) { *s = '\0'; rest = trim(s + 1); } else rest = s;
    for (i = 0; mnemonic[i]; i++) mnemonic[i] = (char)toupper((unsigned char)mnemonic[i]);

    /* NAME EQU expr */
    if (rest && strncmp(rest, "EQU", 3) == 0 && (rest[3] == ' ' || rest[3] == '\t')) {
        long v = evaluate(trim(rest + 3), &ok);
        if (ok && sym_set(mnemonic, v)) changed = 1;
        else if (!ok && emitting) err("EQU with undefined symbol: %s", mnemonic);
        return;
    }

    nops = split_operands(rest, ops, 8);

    /* directives */
    if (strcmp(mnemonic, "ORG") == 0) {
        long v = nops ? evaluate(ops[0], &ok) : 0;
        if (ok) pc = v;
        return;
    }
    if (strcmp(mnemonic, "END") == 0) return;
    if (strcmp(mnemonic, "DS") == 0) {
        long v = nops ? evaluate(ops[0], &ok) : 0;
        pc += v;
        return;
    }
    if (strcmp(mnemonic, "DB") == 0) {
        for (i = 0; i < nops; i++) {
            long v = evaluate(ops[i], &ok);
            if (!ok && emitting) err("undefined symbol in DB: %s", ops[i]);
            emit_byte((unsigned char)(v & 0xFF));
        }
        return;
    }
    if (strcmp(mnemonic, "DW") == 0) {
        for (i = 0; i < nops; i++) {
            long v = evaluate(ops[i], &ok);
            if (!ok && emitting) err("undefined symbol in DW: %s", ops[i]);
            emit_word(v);
        }
        return;
    }

    /* instructions */
    for (i = 0; i < MNEM_N; i++) {
        if (strcmp(MNEM[i].name, mnemonic) != 0) continue;
        {
            const mnem_t *m = &MNEM[i];
            long v;
            int r, r2;

            for (r = 0; r < nops; r++) {
                char *p = ops[r];
                /* Register names are compared in uppercase; expressions are
                 * normalised by the evaluator itself. */
                if (strlen(p) <= 3) { int k; for (k = 0; p[k]; k++) p[k] = (char)toupper((unsigned char)p[k]); }
            }

            switch (m->kind) {
                case K_NONE:
                    emit_byte(m->base);
                    return;
                case K_R8D:
                    r = reg8(nops ? ops[0] : NULL);
                    if (r < 0) { if (emitting) err("invalid register in %s", mnemonic); r = 0; }
                    emit_byte((unsigned char)(m->base + (r << 3)));
                    return;
                case K_R8S:
                    r = reg8(nops ? ops[0] : NULL);
                    if (r < 0) { if (emitting) err("invalid register in %s", mnemonic); r = 0; }
                    emit_byte((unsigned char)(m->base + r));
                    return;
                case K_MOV:
                    r  = reg8(nops > 0 ? ops[0] : NULL);
                    r2 = reg8(nops > 1 ? ops[1] : NULL);
                    if (r < 0 || r2 < 0) { if (emitting) err("invalid register in %s", mnemonic); r = r2 = 0; }
                    emit_byte((unsigned char)(0x40 + (r << 3) + r2));
                    return;
                case K_MVI:
                    r = reg8(nops > 0 ? ops[0] : NULL);
                    if (r < 0) { if (emitting) err("invalid register in %s", mnemonic); r = 0; }
                    v = (nops > 1) ? evaluate(ops[1], &ok) : 0;
                    if (!ok && emitting) err("undefined symbol in %s", mnemonic);
                    emit_byte((unsigned char)(m->base + (r << 3)));
                    emit_byte((unsigned char)(v & 0xFF));
                    return;
                case K_LXI:
                    r = regpair(nops > 0 ? ops[0] : NULL, 0);
                    if (r < 0) { if (emitting) err("invalid register pair in %s", mnemonic); r = 0; }
                    v = (nops > 1) ? evaluate(ops[1], &ok) : 0;
                    if (!ok && emitting) err("undefined symbol in %s", mnemonic);
                    emit_byte((unsigned char)(0x01 + (r << 4)));
                    emit_word(v);
                    return;
                case K_RP:
                    r = regpair(nops ? ops[0] : NULL, 0);
                    if (r < 0) { if (emitting) err("invalid register pair in %s", mnemonic); r = 0; }
                    emit_byte((unsigned char)(m->base + (r << 4)));
                    return;
                case K_STK:
                    r = regpair(nops ? ops[0] : NULL, 1);
                    if (r < 0) { if (emitting) err("invalid register pair in %s", mnemonic); r = 0; }
                    emit_byte((unsigned char)(m->base + (r << 4)));
                    return;
                case K_AX:
                    r = regpair(nops ? ops[0] : NULL, 0);
                    if (r != 0 && r != 1) { if (emitting) err("%s only accepts B or D", mnemonic); r = 0; }
                    emit_byte((unsigned char)(m->base + (r << 4)));
                    return;
                case K_IMM8:
                    v = nops ? evaluate(ops[0], &ok) : 0;
                    if (!ok && emitting) err("undefined symbol in %s", mnemonic);
                    emit_byte(m->base);
                    emit_byte((unsigned char)(v & 0xFF));
                    return;
                case K_ADDR:
                    v = nops ? evaluate(ops[0], &ok) : 0;
                    if (!ok && emitting) err("undefined symbol in %s", mnemonic);
                    emit_byte(m->base);
                    emit_word(v);
                    return;
                case K_RST:
                    v = nops ? evaluate(ops[0], &ok) : 0;
                    emit_byte((unsigned char)(0xC7 + ((v & 7) << 3)));
                    return;
                case K_PORT:
                    v = nops ? evaluate(ops[0], &ok) : 0;
                    if (!ok && emitting) err("undefined symbol in %s", mnemonic);
                    emit_byte(m->base);
                    emit_byte((unsigned char)(v & 0xFF));
                    return;
            }
        }
    }

    if (emitting) err("unknown mnemonic: %s", mnemonic);
}

/* --- main program --------------------------------------------------------- */

int main(int argc, char **argv)
{
    FILE *f;
    const char *src = NULL, *dst = NULL, *verify = NULL;
    int pass, i;

    console_utf8();

    

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) image_size = strtol(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--verify") == 0 && i + 1 < argc) verify = argv[++i];
        else if (!src) src = argv[i];
        else if (!dst) dst = argv[i];
    }
    if (!src || !dst) {
        fprintf(stderr, "usage: %s <source.asm> <output.bin> [--size N] [--verify <ref.bin>]\n",
                argv[0]);
        return 2;
    }

    f = fopen(src, "rb");
    if (!f) { perror(src); return 2; }
    while (nlines < MAX_LINES && fgets(lines[nlines], MAX_LINE, f)) nlines++;
    fclose(f);

    memset(image, 0xFF, sizeof image);

    /* Repeat passes until the symbols settle: the source has
     * forward-referencing EQUs. */
    for (pass = 0; pass < 16; pass++) {
        changed = 0;
        emitting = 0;
        pc = 0;
        for (cur_line = 0; cur_line < nlines; cur_line++) assemble_line(lines[cur_line]);
        if (!changed) break;
    }
    if (pass >= 16) {
        fprintf(stderr, "symbols do not converge after 16 passes\n");
        return 1;
    }

    emitting = 1;
    pc = 0;
    errors = 0;
    for (cur_line = 0; cur_line < nlines; cur_line++) assemble_line(lines[cur_line]);

    printf("%d lines, %d symbols, %d passes to converge\n",
           nlines, nsyms, pass + 1);

    if (errors) { fprintf(stderr, "%d errors\n", errors); return 1; }

    f = fopen(dst, "wb");
    if (!f) { perror(dst); return 2; }
    fwrite(image, 1, (size_t)image_size, f);
    fclose(f);
    printf("%s written (%ld bytes)\n", dst, image_size);

    /* Second path of PLAN.md §4: the reassembled image must match, byte for
     * byte, the one rebuilt from the listing's OBJ column. */
    if (verify) {
        static unsigned char ref[IMAGE_MAX];
        size_t n;
        long k, diff = 0;

        f = fopen(verify, "rb");
        if (!f) { perror(verify); return 2; }
        n = fread(ref, 1, sizeof ref, f);
        fclose(f);

        if ((long)n != image_size) {
            printf("VERIFICATION FAILED: %s is %lu bytes, the image is %ld\n",
                   verify, (unsigned long)n, image_size);
            return 1;
        }
        for (k = 0; k < image_size; k++) {
            if (image[k] != ref[k]) {
                if (diff < 20)
                    printf("  %04lXH  reassembled=%02X  OBJ column=%02X\n",
                           k, image[k], ref[k]);
                diff++;
            }
        }
        if (diff) {
            printf("VERIFICATION FAILED: %ld bytes differ\n", diff);
            return 1;
        }
        printf("VERIFICATION OK: reassembled == OBJ column, %ld bytes identical\n",
               image_size);
    }
    return 0;
}


