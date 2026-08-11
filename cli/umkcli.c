/* umkcli.c — modo sin ventana y depurador del УМК-80.
 *
 * Cubre tres entregables del encargo:
 *   3. modo headless: cargar un binario en una dirección, ejecutar N ciclos
 *      y volcar registros, memoria y estado del display;
 *   5. depurador: puntos de ruptura, paso por instrucción y por ciclo de
 *      máquina, inspección y edición de registros y memoria;
 *   6. guardar y restaurar el estado completo de la máquina.
 *
 * Además carga y escribe Intel HEX (entregable 4).
 *
 * Se puede usar de dos maneras:
 *   - por lotes, encadenando órdenes:      umkcli -c "load p.bin 0800" -c "run 1e6" -c regs
 *   - interactivo, sin órdenes en la línea: umkcli --rom rom/monitor.bin
 */

#include "umk80/umk80.h"
#include "disasm.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_BP 16

static umk_machine_t m;
static uint16_t breakpoints[MAX_BP];
static int      nbp;
static int      quit_requested;

/* --- utilidades ------------------------------------------------------------- */

static long parse_num(const char *s, long def)
{
    char *e;
    long v;
    if (!s || !*s) return def;
    /* Se admite 0x1234, 1234H y decimal. Sin prefijo se toma hexadecimal,
     * que es lo natural delante de un panel que sólo habla en hex. */
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return strtol(s + 2, NULL, 16);
    v = strtol(s, &e, 16);
    if (*e == 'H' || *e == 'h' || *e == '\0') return v;
    return strtol(s, NULL, 0);
}

static const char *flags_text(uint8_t f)
{
    static char buf[16];
    snprintf(buf, sizeof buf, "%c%c%c%c%c",
             (f & I8080_F_S)  ? 'S' : '.',
             (f & I8080_F_Z)  ? 'Z' : '.',
             (f & I8080_F_AC) ? 'A' : '.',
             (f & I8080_F_P)  ? 'P' : '.',
             (f & I8080_F_C)  ? 'C' : '.');
    return buf;
}

static void show_regs(void)
{
    unsigned char mem[3];
    char txt[64];
    unsigned i;
    for (i = 0; i < 3u; i++) mem[i] = umk_peek(&m, (uint16_t)(m.cpu.pc + i));
    umk_disasm(mem, 0, 3, txt, sizeof txt);

    printf("A=%02X F=%02X[%s] B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X\n",
           m.cpu.a, m.cpu.f, flags_text(m.cpu.f),
           m.cpu.b, m.cpu.c, m.cpu.d, m.cpu.e, m.cpu.h, m.cpu.l);
    printf("PC=%04X SP=%04X  INTE=%d HLTA=%d  ciclos=%llu\n",
           m.cpu.pc, m.cpu.sp, m.cpu.inte ? 1 : 0, m.cpu.halted ? 1 : 0,
           (unsigned long long)m.cpu.cycles);
    printf("PC:  %04X  %s\n", m.cpu.pc, txt);
}

static void show_mem(uint16_t addr, unsigned len)
{
    unsigned i, j;
    for (i = 0; i < len; i += 16u) {
        printf("%04X  ", (unsigned)(addr + i));
        for (j = 0; j < 16u; j++) {
            if (i + j < len) printf("%02X ", umk_peek(&m, (uint16_t)(addr + i + j)));
            else printf("   ");
            if (j == 7u) putchar(' ');
        }
        printf(" |");
        for (j = 0; j < 16u && i + j < len; j++) {
            uint8_t c = umk_peek(&m, (uint16_t)(addr + i + j));
            putchar((c >= 32u && c < 127u) ? (char)c : '.');
        }
        printf("|\n");
    }
}

/* Dibuja los seis indicadores en tres líneas de texto: en modo sin ventana
 * es la única forma de ver lo que muestra la máquina. */
static void show_display(void)
{
    uint8_t pat[UMK_DIGITS];
    uint8_t rel[UMK_DIGITS][UMK_SEGMENTS];
    unsigned d, row;

    umk_display_pattern(&m, 25u, pat);
    umk_display_relative(&m, rel);

    for (row = 0; row < 3u; row++) {
        printf("  ");
        for (d = 0; d < UMK_DIGITS; d++) {
            uint8_t p = pat[d];
            if (row == 0) printf("%s", (p & 0x01u) ? " _ " : "   ");
            else if (row == 1)
                printf("%c%c%c", (p & 0x20u) ? '|' : ' ',
                                 (p & 0x40u) ? '_' : ' ',
                                 (p & 0x02u) ? '|' : ' ');
            else
                printf("%c%c%c", (p & 0x10u) ? '|' : ' ',
                                 (p & 0x08u) ? '_' : ' ',
                                 (p & 0x04u) ? '|' : ' ');
            printf(" ");
            if (d == 3u) printf("  ");      /* separación АДРЕС | ДАННЫЕ */
        }
        putchar('\n');
    }
    printf("  patrón:");
    for (d = 0; d < UMK_DIGITS; d++) printf(" %02X", pat[d]);
    printf("   (izquierda -> derecha)\n");

    printf("  brillo relativo por dígito (0..255), segmentos A..G:\n");
    for (d = 0; d < UMK_DIGITS; d++) {
        unsigned s;
        printf("    %u:", d);
        for (s = 0; s < 7u; s++) printf(" %3u", rel[d][s]);
        putchar('\n');
    }
}

static void show_panel(void)
{
    static const char *const NAMES[8] =
        { "INTA","WO","STACK","HLTA","OUT","M1","INP","MEMR" };
    int i;
    printf("АДРЕС    = %04X\n", m.panel.address);
    printf("ДАННЫЕ   = %02X\n", m.panel.data);
    printf("СОСТОЯНИЕ= %02X  ", m.panel.status);
    for (i = 7; i >= 0; i--)
        if (m.panel.status & (1u << i)) printf("%s ", NAMES[i]);
    putchar('\n');
    printf("avería +5V=%d -5V=%d +12V=%d   (encendido = esa tensión FALTA)\n",
           m.panel.fault_p5, m.panel.fault_m5, m.panel.fault_p12);
    printf("РБ/ШГ=%s  КМ/ЦК=%s  puerto 0FCh=%02X\n",
           umk_get_switch(&m, UMK_SW_STEP)  ? "enclavado" : "suelto",
           umk_get_switch(&m, UMK_SW_CYCLE) ? "enclavado" : "suelto",
           m.step.dbg_port);
}

/* --- ficheros ---------------------------------------------------------------- */

static int load_bin(const char *path, uint16_t addr, int into_rom)
{
    static uint8_t buf[65536];
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) { perror(path); return 0; }
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    if (into_rom) {
        if (!umk_load_rom(&m, addr, buf, n)) {
            printf("no cabe en el ПЗУ\n");
            return 0;
        }
    } else {
        umk_load_ram(&m, addr, buf, n);
    }
    printf("%s: %lu bytes en %04X (%s)\n", path, (unsigned long)n, addr,
           into_rom ? "ПЗУ" : "ОЗУ");
    return 1;
}

static int hexb(const char *s)
{
    int hi = -1, lo = -1;
    if (isxdigit((unsigned char)s[0]) && isxdigit((unsigned char)s[1])) {
        char t[3]; t[0] = s[0]; t[1] = s[1]; t[2] = 0;
        hi = (int)strtol(t, NULL, 16);
        lo = 0;
    }
    (void)lo;
    return hi;
}

static int load_hex(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[600];
    unsigned long total = 0;
    int lineno = 0;

    if (!f) { perror(path); return 0; }
    while (fgets(line, sizeof line, f)) {
        int len, type, i, sum = 0, want;
        unsigned addr;
        lineno++;
        if (line[0] != ':') continue;

        len  = hexb(line + 1);
        if (len < 0) { printf("línea %d: registro ilegible\n", lineno); fclose(f); return 0; }
        addr = (unsigned)((hexb(line + 3) << 8) | hexb(line + 5));
        type = hexb(line + 7);

        sum = len + (int)((addr >> 8) & 0xFF) + (int)(addr & 0xFF) + type;
        for (i = 0; i < len; i++) {
            int b = hexb(line + 9 + i * 2);
            sum += b;
            if (type == 0) umk_poke(&m, (uint16_t)(addr + i), (uint8_t)b);
        }
        want = hexb(line + 9 + len * 2);
        if (((sum + want) & 0xFF) != 0) {
            printf("línea %d: suma de verificación incorrecta\n", lineno);
            fclose(f);
            return 0;
        }
        if (type == 0) total += (unsigned long)len;
        else if (type == 1) break;
        else printf("línea %d: registro de tipo %02X ignorado\n", lineno, type);
    }
    fclose(f);
    printf("%s: %lu bytes cargados (Intel HEX)\n", path, total);
    return 1;
}

static int save_bin(const char *path, uint16_t addr, unsigned len)
{
    FILE *f = fopen(path, "wb");
    unsigned i;
    if (!f) { perror(path); return 0; }
    for (i = 0; i < len; i++) fputc(umk_peek(&m, (uint16_t)(addr + i)), f);
    fclose(f);
    printf("%s: %u bytes desde %04X\n", path, len, addr);
    return 1;
}

static int save_hex(const char *path, uint16_t addr, unsigned len)
{
    FILE *f = fopen(path, "w");
    unsigned i;
    if (!f) { perror(path); return 0; }
    for (i = 0; i < len; i += 16u) {
        unsigned n = (len - i < 16u) ? (len - i) : 16u;
        unsigned a = (unsigned)(addr + i), k, sum;
        fprintf(f, ":%02X%04X00", n, a);
        sum = n + ((a >> 8) & 0xFF) + (a & 0xFF);
        for (k = 0; k < n; k++) {
            uint8_t b = umk_peek(&m, (uint16_t)(a + k));
            fprintf(f, "%02X", b);
            sum += b;
        }
        fprintf(f, "%02X\n", (unsigned)((0x100u - (sum & 0xFFu)) & 0xFFu));
    }
    fprintf(f, ":00000001FF\n");
    fclose(f);
    printf("%s: %u bytes desde %04X (Intel HEX)\n", path, len, addr);
    return 1;
}

/* --- puntos de ruptura -------------------------------------------------------- */

static int bp_hit(uint16_t pc)
{
    int i;
    for (i = 0; i < nbp; i++) if (breakpoints[i] == pc) return 1;
    return 0;
}

/* --- teclas -------------------------------------------------------------------- */

typedef struct { const char *name; int col, row; } keyname_t;

static const keyname_t KEYS[] = {
    {"0",2,1},{"1",3,1},{"2",4,1},{"3",5,1},
    {"4",2,3},{"5",3,3},{"6",4,3},{"7",5,3},
    {"8",2,2},{"9",3,2},{"A",4,2},{"B",5,2},
    {"C",2,0},{"D",3,0},{"E",4,0},{"F",5,0},
    {"P",0,1},{"RG",1,1},{"ST",0,3},{"KS",1,3},
    {"ZK",0,2},{"PM",1,2},{"SPACE",0,0},{"_",0,0},{"VP",1,0}
};
#define KEYS_N ((int)(sizeof KEYS / sizeof KEYS[0]))

#define KEY_HOLD 120000u

static int press_key(const char *name)
{
    int i;
    for (i = 0; i < KEYS_N; i++) {
        if (strcmp(KEYS[i].name, name) == 0) {
            umk_set_key(&m, (unsigned)KEYS[i].col, (unsigned)KEYS[i].row, true);
            umk_run_cycles(&m, KEY_HOLD);
            umk_set_key(&m, (unsigned)KEYS[i].col, (unsigned)KEYS[i].row, false);
            umk_run_cycles(&m, KEY_HOLD);
            return 1;
        }
    }
    printf("tecla desconocida: %s\n", name);
    return 0;
}

/* --- órdenes -------------------------------------------------------------------- */

static void usage(void)
{
    printf(
"Órdenes (los números son hexadecimales salvo que lleven 0x o sufijo):\n"
"  rom <f> [off]        carga imagen de ПЗУ\n"
"  load <f> [dir]       carga .bin en ОЗУ (por omisión 0800)\n"
"  loadhex <f>          carga Intel HEX\n"
"  save <f> <dir> <n>   vuelca memoria a .bin\n"
"  savehex <f> <dir> <n>\n"
"  run [ciclos]         corre hasta el tope o hasta un punto de ruptura\n"
"  go <dir> [ciclos]    fija PC y corre\n"
"  step [n]             n instrucciones (por omisión 1)\n"
"  cycle [n]            n ciclos de máquina\n"
"  reset                pulsador СБ\n"
"  int                  pulsador ПР (RST 7)\n"
"  key <nombre>         pulsa y suelta: 0..F, P, RG, ST, KS, ZK, PM, SPACE, VP\n"
"  keys <cadena>        varias seguidas, separadas por comas\n"
"  sw step|cycle on|off РБ/ШГ y КМ/ЦК\n"
"  shg                  pulsador ШГ\n"
"  regs                 registros y la instrucción en PC\n"
"  reg <r> <v>          fija A B C D E H L F PC SP\n"
"  mem <dir> [n]        volcado hexadecimal\n"
"  poke <dir> <b>...    escribe bytes\n"
"  dis [dir] [n]        desensambla\n"
"  bp <dir> | bp list | bp del <dir> | bp clear\n"
"  display              lo que muestran los seis indicadores\n"
"  panel                LEDs АДРЕС, ДАННЫЕ, СОСТОЯНИЕ y conmutadores\n"
"  state save <f> | state load <f>\n"
"  help | quit\n");
}

static void set_reg(const char *r, long v)
{
    if      (!strcmp(r,"A"))  m.cpu.a = (uint8_t)v;
    else if (!strcmp(r,"B"))  m.cpu.b = (uint8_t)v;
    else if (!strcmp(r,"C"))  m.cpu.c = (uint8_t)v;
    else if (!strcmp(r,"D"))  m.cpu.d = (uint8_t)v;
    else if (!strcmp(r,"E"))  m.cpu.e = (uint8_t)v;
    else if (!strcmp(r,"H"))  m.cpu.h = (uint8_t)v;
    else if (!strcmp(r,"L"))  m.cpu.l = (uint8_t)v;
    else if (!strcmp(r,"F"))  m.cpu.f = (uint8_t)((v & I8080_F_MASK) | I8080_F_ONE);
    else if (!strcmp(r,"PC")) m.cpu.pc = (uint16_t)v;
    else if (!strcmp(r,"SP")) m.cpu.sp = (uint16_t)v;
    else { printf("registro desconocido: %s\n", r); return; }
    printf("%s = %lX\n", r, v);
}

static void do_run(uint64_t budget)
{
    uint64_t start = m.cpu.cycles;
    unsigned long steps = 0;
    while (m.cpu.cycles - start < budget) {
        umk_step_instruction(&m);
        steps++;
        if (bp_hit(m.cpu.pc)) {
            printf("punto de ruptura en %04X tras %lu instrucciones\n",
                   m.cpu.pc, steps);
            show_regs();
            return;
        }
        if (m.cpu.halted && !m.cpu.int_pending) {
            printf("HLT en %04X tras %lu instrucciones\n", m.cpu.pc, steps);
            return;
        }
    }
    printf("%llu ciclos consumidos (%lu instrucciones)\n",
           (unsigned long long)(m.cpu.cycles - start), steps);
}

static void do_dis(uint16_t addr, unsigned count)
{
    static unsigned char buf[65536];
    unsigned i;
    char txt[64];
    for (i = 0; i < 65536u; i++) buf[i] = umk_peek(&m, (uint16_t)i);
    for (i = 0; i < count; i++) {
        unsigned len = umk_disasm(buf, addr, 65536u, txt, sizeof txt);
        unsigned k;
        printf("%04X  ", addr);
        for (k = 0; k < 3u; k++) {
            if (k < len) printf("%02X ", buf[addr + k]);
            else printf("   ");
        }
        printf(" %s%s\n", txt, bp_hit(addr) ? "   <- punto de ruptura" : "");
        addr = (uint16_t)(addr + len);
    }
}

static void execute(char *line)
{
    char *tok[8];
    int n = 0;
    char *p = strtok(line, " \t\r\n");
    while (p && n < 8) { tok[n++] = p; p = strtok(NULL, " \t\r\n"); }
    if (!n) return;
    if (tok[0][0] == '#' || tok[0][0] == ';') return;

    /* las órdenes en minúsculas, los argumentos tal cual */
    { int i; for (i = 0; tok[0][i]; i++) tok[0][i] = (char)tolower((unsigned char)tok[0][i]); }

    if (!strcmp(tok[0], "help") || !strcmp(tok[0], "?")) usage();
    else if (!strcmp(tok[0], "quit") || !strcmp(tok[0], "q")) quit_requested = 1;
    else if (!strcmp(tok[0], "rom") && n >= 2)
        load_bin(tok[1], (uint16_t)parse_num(n > 2 ? tok[2] : NULL, 0), 1);
    else if (!strcmp(tok[0], "load") && n >= 2)
        load_bin(tok[1], (uint16_t)parse_num(n > 2 ? tok[2] : NULL, 0x0800), 0);
    else if (!strcmp(tok[0], "loadhex") && n >= 2) load_hex(tok[1]);
    else if (!strcmp(tok[0], "save") && n >= 4)
        save_bin(tok[1], (uint16_t)parse_num(tok[2], 0), (unsigned)parse_num(tok[3], 0));
    else if (!strcmp(tok[0], "savehex") && n >= 4)
        save_hex(tok[1], (uint16_t)parse_num(tok[2], 0), (unsigned)parse_num(tok[3], 0));
    else if (!strcmp(tok[0], "run"))
        do_run(n > 1 ? (uint64_t)strtoull(tok[1], NULL, 0) : 2000000ull);
    else if (!strcmp(tok[0], "go") && n >= 2) {
        m.cpu.pc = (uint16_t)parse_num(tok[1], 0);
        do_run(n > 2 ? (uint64_t)strtoull(tok[2], NULL, 0) : 2000000ull);
    }
    else if (!strcmp(tok[0], "step") || !strcmp(tok[0], "s")) {
        long k = n > 1 ? parse_num(tok[1], 1) : 1;
        while (k-- > 0) umk_step_instruction(&m);
        show_regs();
    }
    else if (!strcmp(tok[0], "cycle")) {
        long k = n > 1 ? parse_num(tok[1], 1) : 1;
        while (k-- > 0) {
            bool last = umk_step_machine_cycle(&m);
            printf("АДРЕС=%04X ДАННЫЕ=%02X СОСТОЯНИЕ=%02X%s\n",
                   m.panel.address, m.panel.data, m.panel.status,
                   last ? "   <- fin de instrucción" : "");
        }
    }
    else if (!strcmp(tok[0], "reset")) { umk_reset(&m); printf("СБ\n"); }
    else if (!strcmp(tok[0], "int"))   { umk_interrupt(&m); printf("ПР\n"); }
    else if (!strcmp(tok[0], "shg"))   { umk_press_step(&m); show_regs(); }
    else if (!strcmp(tok[0], "key") && n >= 2)  press_key(tok[1]);
    else if (!strcmp(tok[0], "keys") && n >= 2) {
        char *k = strtok(tok[1], ",");
        while (k) { if (!press_key(k)) break; k = strtok(NULL, ","); }
    }
    else if (!strcmp(tok[0], "sw") && n >= 3) {
        int on = !strcmp(tok[2], "on");
        if (!strcmp(tok[1], "step"))       umk_set_switch(&m, UMK_SW_STEP, on);
        else if (!strcmp(tok[1], "cycle")) umk_set_switch(&m, UMK_SW_CYCLE, on);
        else { printf("conmutador desconocido: %s\n", tok[1]); return; }
        printf("РБ/ШГ=%s КМ/ЦК=%s\n",
               umk_get_switch(&m, UMK_SW_STEP) ? "enclavado" : "suelto",
               umk_get_switch(&m, UMK_SW_CYCLE) ? "enclavado" : "suelto");
    }
    else if (!strcmp(tok[0], "regs") || !strcmp(tok[0], "r")) show_regs();
    else if (!strcmp(tok[0], "reg") && n >= 3) {
        char rn[8]; int i;
        for (i = 0; i < 7 && tok[1][i]; i++) rn[i] = (char)toupper((unsigned char)tok[1][i]);
        rn[i] = '\0';
        set_reg(rn, parse_num(tok[2], 0));
    }
    else if (!strcmp(tok[0], "mem") && n >= 2)
        show_mem((uint16_t)parse_num(tok[1], 0), (unsigned)(n > 2 ? parse_num(tok[2], 64) : 64));
    else if (!strcmp(tok[0], "poke") && n >= 3) {
        uint16_t a = (uint16_t)parse_num(tok[1], 0);
        int i;
        for (i = 2; i < n; i++) umk_poke(&m, (uint16_t)(a + i - 2), (uint8_t)parse_num(tok[i], 0));
        printf("%d bytes en %04X\n", n - 2, a);
    }
    else if (!strcmp(tok[0], "dis"))
        do_dis((uint16_t)(n > 1 ? parse_num(tok[1], 0) : m.cpu.pc),
               (unsigned)(n > 2 ? parse_num(tok[2], 16) : 16));
    else if (!strcmp(tok[0], "bp")) {
        if (n == 1 || !strcmp(tok[1], "list")) {
            int i;
            if (!nbp) printf("sin puntos de ruptura\n");
            for (i = 0; i < nbp; i++) printf("  %04X\n", breakpoints[i]);
        } else if (!strcmp(tok[1], "clear")) { nbp = 0; printf("borrados\n"); }
        else if (!strcmp(tok[1], "del") && n >= 3) {
            uint16_t a = (uint16_t)parse_num(tok[2], 0);
            int i, j = 0;
            for (i = 0; i < nbp; i++) if (breakpoints[i] != a) breakpoints[j++] = breakpoints[i];
            nbp = j;
            printf("quitado %04X\n", a);
        } else if (nbp < MAX_BP) {
            breakpoints[nbp++] = (uint16_t)parse_num(tok[1], 0);
            printf("punto de ruptura en %04X\n", breakpoints[nbp - 1]);
        } else printf("no caben más de %d puntos de ruptura\n", MAX_BP);
    }
    else if (!strcmp(tok[0], "display") || !strcmp(tok[0], "d")) show_display();
    else if (!strcmp(tok[0], "panel")) show_panel();
    else if (!strcmp(tok[0], "state") && n >= 3) {
        static unsigned char buf[sizeof(umk_machine_t)];
        FILE *f;
        if (!strcmp(tok[1], "save")) {
            if (!umk_state_save(&m, buf, sizeof buf)) { printf("no se pudo guardar\n"); return; }
            f = fopen(tok[2], "wb");
            if (!f) { perror(tok[2]); return; }
            fwrite(buf, 1, umk_state_size(), f);
            fclose(f);
            printf("estado guardado en %s (%lu bytes)\n", tok[2],
                   (unsigned long)umk_state_size());
        } else if (!strcmp(tok[1], "load")) {
            size_t got;
            f = fopen(tok[2], "rb");
            if (!f) { perror(tok[2]); return; }
            got = fread(buf, 1, sizeof buf, f);
            fclose(f);
            if (got != umk_state_size() || !umk_state_load(&m, buf, got))
                printf("el fichero no es un estado válido de esta versión\n");
            else printf("estado restaurado desde %s\n", tok[2]);
        } else printf("state save|load <fichero>\n");
    }
    else printf("orden desconocida: %s  (help para la lista)\n", tok[0]);
}

/* --- programa principal ---------------------------------------------------------- */

int main(int argc, char **argv)
{
    char line[512];
    const char *cmds[64];
    int ncmds = 0, i, interactive = 1;
    const char *script = NULL;

    console_utf8();
    umk_init(&m, UMK_REV2);

    for (i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-c") || !strcmp(argv[i], "--cmd")) && i + 1 < argc) {
            if (ncmds < 64) cmds[ncmds++] = argv[++i];
            interactive = 0;
        } else if (!strcmp(argv[i], "--script") && i + 1 < argc) {
            script = argv[++i];
            interactive = 0;
        } else if (!strcmp(argv[i], "--rom") && i + 1 < argc) {
            load_bin(argv[++i], 0, 1);
            umk_reset(&m);
        } else if (!strcmp(argv[i], "--rev1")) {
            umk_init(&m, UMK_REV1);
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("uso: %s [--rom <f>] [--rev1] [-c \"orden\"]... [--script <f>]\n\n",
                   argv[0]);
            usage();
            return 0;
        }
    }

    for (i = 0; i < ncmds && !quit_requested; i++) {
        strncpy(line, cmds[i], sizeof line - 1);
        line[sizeof line - 1] = '\0';
        execute(line);
    }

    if (script) {
        FILE *f = fopen(script, "r");
        if (!f) { perror(script); return 2; }
        while (!quit_requested && fgets(line, sizeof line, f)) execute(line);
        fclose(f);
    }

    if (interactive) {
        printf("УМК-80 — depurador. «help» para la lista de órdenes.\n");
        while (!quit_requested) {
            printf("umk> ");
            fflush(stdout);
            if (!fgets(line, sizeof line, stdin)) break;
            execute(line);
        }
    }
    return 0;
}

