/* i8080.c — intérprete del Intel 8080 / КР580ВМ80А.
 *
 * Objetivos, por este orden:
 *   1. Exactitud de banderas, incluidas AC y las peculiaridades del 8080
 *      frente al Z80 (ANA fija AC según (a|b)&8; los bits 3 y 5 del PSW
 *      valen siempre 0 y el bit 1 siempre 1).
 *   2. Exactitud de ciclos T por instrucción, incluido el camino tomado y
 *      no tomado de saltos, llamadas y retornos condicionales.
 *   3. Traza de ciclos de máquina con su palabra de estado, que es lo que
 *      alimenta la fila de LEDs «СОСТОЯНИЕ» y el modo paso por ciclo.
 *
 * Los diez opcodes no documentados se implementan con la semántica del
 * 8080 de Intel: 08/10/18/20/28/30/38 son NOP, CB es JMP, D9 es RET y
 * DD/ED/FD son CALL.
 */

#include "umk80/i8080.h"

/* --- utilidades ---------------------------------------------------------- */

static bool parity8(uint8_t v)
{
    v ^= (uint8_t)(v >> 4);
    v ^= (uint8_t)(v >> 2);
    v ^= (uint8_t)(v >> 1);
    return (v & 1u) == 0u;   /* paridad par -> P = 1 */
}

/* Acarreo que sale del bit `bit` al sumar a + b + cy. */
static bool carry_out(unsigned bit, uint8_t a, uint8_t b, bool cy)
{
    uint16_t res = (uint16_t)(a + b + (cy ? 1u : 0u));
    uint16_t mix = (uint16_t)(res ^ a ^ b);
    return (mix & (1u << bit)) != 0u;
}

static void set_szp(i8080_t *c, uint8_t v)
{
    c->f = (uint8_t)(c->f & ~(I8080_F_S | I8080_F_Z | I8080_F_P));
    if (v & 0x80u)      c->f |= I8080_F_S;
    if (v == 0u)        c->f |= I8080_F_Z;
    if (parity8(v))     c->f |= I8080_F_P;
    /* Invariante del 8080: bit 1 siempre a 1, bits 3 y 5 siempre a 0. */
    c->f = (uint8_t)((c->f & I8080_F_MASK) | I8080_F_ONE);
}

static void set_flag(i8080_t *c, uint8_t mask, bool on)
{
    if (on) c->f |= mask; else c->f = (uint8_t)(c->f & ~mask);
}

static bool get_flag(const i8080_t *c, uint8_t mask)
{
    return (c->f & mask) != 0u;
}

/* --- acceso al bus, con registro del ciclo de máquina --------------------- */

static void mc_log(i8080_t *c, uint8_t status, uint16_t addr, uint8_t data)
{
    if (c->mc_count < (uint8_t)(sizeof c->mc_status)) {
        c->mc_status[c->mc_count] = status;
        c->mc_addr[c->mc_count]   = addr;
        c->mc_data[c->mc_count]   = data;
        c->mc_count++;
    }
}

static uint8_t bus_rd(i8080_t *c, const i8080_bus_t *b, uint16_t a, uint8_t st)
{
    uint8_t v = b->read(b->ud, a, st);
    mc_log(c, st, a, v);
    return v;
}

static void bus_wr(i8080_t *c, const i8080_bus_t *b, uint16_t a, uint8_t v, uint8_t st)
{
    b->write(b->ud, a, v, st);
    mc_log(c, st, a, v);
}

/* En el 8080 el número de puerto sale duplicado en A0-A7 y en A8-A15. */
static uint8_t bus_in(i8080_t *c, const i8080_bus_t *b, uint8_t port)
{
    uint8_t v = b->in(b->ud, port);
    mc_log(c, I8080_CYC_INPR, (uint16_t)((port << 8) | port), v);
    return v;
}

static void bus_out(i8080_t *c, const i8080_bus_t *b, uint8_t port, uint8_t v)
{
    b->out(b->ud, port, v);
    mc_log(c, I8080_CYC_OUTW, (uint16_t)((port << 8) | port), v);
}

static uint8_t fetch8(i8080_t *c, const i8080_bus_t *b)
{
    return bus_rd(c, b, c->pc++, I8080_CYC_MEMR);
}

static uint16_t fetch16(i8080_t *c, const i8080_bus_t *b)
{
    uint8_t lo = fetch8(c, b);
    uint8_t hi = fetch8(c, b);
    return (uint16_t)((hi << 8) | lo);
}

static void push16(i8080_t *c, const i8080_bus_t *b, uint16_t v)
{
    bus_wr(c, b, (uint16_t)(c->sp - 1u), (uint8_t)(v >> 8), I8080_CYC_STACKW);
    bus_wr(c, b, (uint16_t)(c->sp - 2u), (uint8_t)(v & 0xFFu), I8080_CYC_STACKW);
    c->sp = (uint16_t)(c->sp - 2u);
}

static uint16_t pop16(i8080_t *c, const i8080_bus_t *b)
{
    uint8_t lo = bus_rd(c, b, c->sp, I8080_CYC_STACKR);
    uint8_t hi = bus_rd(c, b, (uint16_t)(c->sp + 1u), I8080_CYC_STACKR);
    c->sp = (uint16_t)(c->sp + 2u);
    return (uint16_t)((hi << 8) | lo);
}

/* --- pares de registros --------------------------------------------------- */

static uint16_t get_bc(const i8080_t *c) { return (uint16_t)((c->b << 8) | c->c); }
static uint16_t get_de(const i8080_t *c) { return (uint16_t)((c->d << 8) | c->e); }
static uint16_t get_hl(const i8080_t *c) { return (uint16_t)((c->h << 8) | c->l); }
static void set_bc(i8080_t *c, uint16_t v) { c->b = (uint8_t)(v >> 8); c->c = (uint8_t)v; }
static void set_de(i8080_t *c, uint16_t v) { c->d = (uint8_t)(v >> 8); c->e = (uint8_t)v; }
static void set_hl(i8080_t *c, uint16_t v) { c->h = (uint8_t)(v >> 8); c->l = (uint8_t)v; }

/* Registro por índice: 0=B 1=C 2=D 3=E 4=H 5=L 6=M 7=A */
static uint8_t reg_get(i8080_t *c, const i8080_bus_t *b, unsigned r)
{
    switch (r) {
        case 0: return c->b;
        case 1: return c->c;
        case 2: return c->d;
        case 3: return c->e;
        case 4: return c->h;
        case 5: return c->l;
        case 6: return bus_rd(c, b, get_hl(c), I8080_CYC_MEMR);
        default: return c->a;
    }
}

static void reg_set(i8080_t *c, const i8080_bus_t *b, unsigned r, uint8_t v)
{
    switch (r) {
        case 0: c->b = v; break;
        case 1: c->c = v; break;
        case 2: c->d = v; break;
        case 3: c->e = v; break;
        case 4: c->h = v; break;
        case 5: c->l = v; break;
        case 6: bus_wr(c, b, get_hl(c), v, I8080_CYC_MEMW); break;
        default: c->a = v; break;
    }
}

/* --- unidad aritmético-lógica --------------------------------------------- */

static void alu_add(i8080_t *c, uint8_t val, bool cy)
{
    uint8_t res = (uint8_t)(c->a + val + (cy ? 1u : 0u));
    set_flag(c, I8080_F_C,  carry_out(8, c->a, val, cy));
    set_flag(c, I8080_F_AC, carry_out(4, c->a, val, cy));
    set_szp(c, res);
    c->a = res;
}

/* La resta del 8080 es a + ~val + !cy, con el acarreo final invertido.
 * De ahí sale, sin casos especiales, el AC que espera 8080EXM. */
static void alu_sub(i8080_t *c, uint8_t val, bool cy)
{
    alu_add(c, (uint8_t)~val, !cy);
    set_flag(c, I8080_F_C, !get_flag(c, I8080_F_C));
}

static void alu_cmp(i8080_t *c, uint8_t val)
{
    uint8_t saved = c->a;
    alu_sub(c, val, false);
    c->a = saved;
}

static void alu_ana(i8080_t *c, uint8_t val)
{
    /* Peculiaridad del 8080: AC sale de (A | operando) bit 3.
     * El 8085 y el Z80 hacen otra cosa. */
    bool ac = ((c->a | val) & 0x08u) != 0u;
    c->a = (uint8_t)(c->a & val);
    set_szp(c, c->a);
    set_flag(c, I8080_F_C, false);
    set_flag(c, I8080_F_AC, ac);
}

static void alu_xra(i8080_t *c, uint8_t val)
{
    c->a = (uint8_t)(c->a ^ val);
    set_szp(c, c->a);
    set_flag(c, I8080_F_C, false);
    set_flag(c, I8080_F_AC, false);
}

static void alu_ora(i8080_t *c, uint8_t val)
{
    c->a = (uint8_t)(c->a | val);
    set_szp(c, c->a);
    set_flag(c, I8080_F_C, false);
    set_flag(c, I8080_F_AC, false);
}

static void alu_op(i8080_t *c, unsigned op, uint8_t val)
{
    switch (op) {
        case 0: alu_add(c, val, false); break;                      /* ADD */
        case 1: alu_add(c, val, get_flag(c, I8080_F_C)); break;     /* ADC */
        case 2: alu_sub(c, val, false); break;                      /* SUB */
        case 3: alu_sub(c, val, get_flag(c, I8080_F_C)); break;     /* SBB */
        case 4: alu_ana(c, val); break;                             /* ANA */
        case 5: alu_xra(c, val); break;                             /* XRA */
        case 6: alu_ora(c, val); break;                             /* ORA */
        default: alu_cmp(c, val); break;                            /* CMP */
    }
}

static void alu_inr(i8080_t *c, const i8080_bus_t *b, unsigned r)
{
    uint8_t v = reg_get(c, b, r);
    uint8_t res = (uint8_t)(v + 1u);
    set_flag(c, I8080_F_AC, (res & 0x0Fu) == 0x00u);
    set_szp(c, res);
    reg_set(c, b, r, res);
}

static void alu_dcr(i8080_t *c, const i8080_bus_t *b, unsigned r)
{
    uint8_t v = reg_get(c, b, r);
    uint8_t res = (uint8_t)(v - 1u);
    set_flag(c, I8080_F_AC, (res & 0x0Fu) != 0x0Fu);
    set_szp(c, res);
    reg_set(c, b, r, res);
}

static void alu_dad(i8080_t *c, uint16_t val)
{
    uint32_t res = (uint32_t)get_hl(c) + (uint32_t)val;
    set_flag(c, I8080_F_C, (res & 0x10000u) != 0u);
    set_hl(c, (uint16_t)res);
}

static void alu_daa(i8080_t *c)
{
    bool cy = get_flag(c, I8080_F_C);
    uint8_t corr = 0u;
    uint8_t lsb = (uint8_t)(c->a & 0x0Fu);
    uint8_t msb = (uint8_t)(c->a >> 4);

    if (get_flag(c, I8080_F_AC) || lsb > 9u) corr = (uint8_t)(corr + 0x06u);
    if (cy || msb > 9u || (msb >= 9u && lsb > 9u)) {
        corr = (uint8_t)(corr + 0x60u);
        cy = true;
    }
    alu_add(c, corr, false);   /* fija AC a partir de la nibble baja */
    set_flag(c, I8080_F_C, cy);
}

/* --- condiciones ---------------------------------------------------------- */

static bool cond_true(const i8080_t *c, unsigned cc)
{
    switch (cc) {
        case 0: return !get_flag(c, I8080_F_Z);   /* NZ */
        case 1: return  get_flag(c, I8080_F_Z);   /* Z  */
        case 2: return !get_flag(c, I8080_F_C);   /* NC */
        case 3: return  get_flag(c, I8080_F_C);   /* C  */
        case 4: return !get_flag(c, I8080_F_P);   /* PO */
        case 5: return  get_flag(c, I8080_F_P);   /* PE */
        case 6: return !get_flag(c, I8080_F_S);   /* P  */
        default: return get_flag(c, I8080_F_S);   /* M  */
    }
}

/* --- tabla de ciclos T ---------------------------------------------------- */
/* Para saltos, llamadas y retornos condicionales figura el caso NO tomado. */
static const uint8_t CYCLES[256] = {
/*        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
/* 0 */   4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4,
/* 1 */   4, 10,  7,  5,  5,  5,  7,  4,  4, 10,  7,  5,  5,  5,  7,  4,
/* 2 */   4, 10, 16,  5,  5,  5,  7,  4,  4, 10, 16,  5,  5,  5,  7,  4,
/* 3 */   4, 10, 13,  5, 10, 10, 10,  4,  4, 10, 13,  5,  5,  5,  7,  4,
/* 4 */   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/* 5 */   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/* 6 */   5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
/* 7 */   7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5,
/* 8 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/* 9 */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/* A */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/* B */   4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
/* C */   5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11,
/* D */   5, 10, 10, 10, 11, 11,  7, 11,  5, 10, 10, 10, 11, 17,  7, 11,
/* E */   5, 10, 10, 18, 11, 11,  7, 11,  5,  5, 10,  4, 11, 17,  7, 11,
/* F */   5, 10, 10,  4, 11, 11,  7, 11,  5,  5, 10,  4, 11, 17,  7, 11
};

static const uint8_t LENGTHS[256] = {
/*        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
/* 0 */   1,  3,  1,  1,  1,  1,  2,  1,  1,  1,  1,  1,  1,  1,  2,  1,
/* 1 */   1,  3,  1,  1,  1,  1,  2,  1,  1,  1,  1,  1,  1,  1,  2,  1,
/* 2 */   1,  3,  3,  1,  1,  1,  2,  1,  1,  1,  3,  1,  1,  1,  2,  1,
/* 3 */   1,  3,  3,  1,  1,  1,  2,  1,  1,  1,  3,  1,  1,  1,  2,  1,
/* 4 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 5 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 6 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 7 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 8 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 9 */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* A */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* B */   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* C */   1,  1,  3,  3,  3,  1,  2,  1,  1,  1,  3,  3,  3,  3,  2,  1,
/* D */   1,  1,  3,  2,  3,  1,  2,  1,  1,  1,  3,  2,  3,  3,  2,  1,
/* E */   1,  1,  3,  1,  3,  1,  2,  1,  1,  1,  3,  1,  3,  3,  2,  1,
/* F */   1,  1,  3,  1,  3,  1,  2,  1,  1,  1,  3,  1,  3,  3,  2,  1
};

unsigned i8080_opcode_cycles(uint8_t opcode) { return CYCLES[opcode]; }
unsigned i8080_opcode_length(uint8_t opcode) { return LENGTHS[opcode]; }

bool i8080_opcode_undocumented(uint8_t opcode)
{
    switch (opcode) {
        case 0x08: case 0x10: case 0x18: case 0x20:
        case 0x28: case 0x30: case 0x38:            /* NOP alternativos */
        case 0xCB:                                  /* JMP alternativo   */
        case 0xD9:                                  /* RET alternativo   */
        case 0xDD: case 0xED: case 0xFD:            /* CALL alternativos */
            return true;
        default:
            return false;
    }
}

/* --- reset ---------------------------------------------------------------- */

void i8080_reset(i8080_t *cpu)
{
    unsigned i;
    cpu->a = 0; cpu->f = I8080_F_ONE;
    cpu->b = 0; cpu->c = 0; cpu->d = 0; cpu->e = 0; cpu->h = 0; cpu->l = 0;
    cpu->sp = 0; cpu->pc = 0;
    cpu->inte = false;
    cpu->halted = false;
    cpu->int_pending = false;
    cpu->int_vector = 0;
    cpu->int_delay = 0;
    cpu->cycles = 0;
    cpu->mc_count = 0;
    for (i = 0; i < sizeof cpu->mc_status; i++) {
        cpu->mc_status[i] = 0;
        cpu->mc_addr[i] = 0;
        cpu->mc_data[i] = 0;
    }
}

void i8080_interrupt(i8080_t *cpu, uint8_t vector)
{
    cpu->int_pending = true;
    cpu->int_vector = vector;
}

/* --- ejecución ------------------------------------------------------------ */

static unsigned execute(i8080_t *c, const i8080_bus_t *b, uint8_t op)
{
    unsigned cyc = CYCLES[op];

    switch (op) {

    /* --- transferencia de 16 bits --- */
    case 0x01: set_bc(c, fetch16(c, b)); break;              /* LXI B  */
    case 0x11: set_de(c, fetch16(c, b)); break;              /* LXI D  */
    case 0x21: set_hl(c, fetch16(c, b)); break;              /* LXI H  */
    case 0x31: c->sp = fetch16(c, b); break;                 /* LXI SP */

    case 0x02: bus_wr(c, b, get_bc(c), c->a, I8080_CYC_MEMW); break; /* STAX B */
    case 0x12: bus_wr(c, b, get_de(c), c->a, I8080_CYC_MEMW); break; /* STAX D */
    case 0x0A: c->a = bus_rd(c, b, get_bc(c), I8080_CYC_MEMR); break;/* LDAX B */
    case 0x1A: c->a = bus_rd(c, b, get_de(c), I8080_CYC_MEMR); break;/* LDAX D */

    case 0x22: { uint16_t a = fetch16(c, b);                 /* SHLD */
                 bus_wr(c, b, a, c->l, I8080_CYC_MEMW);
                 bus_wr(c, b, (uint16_t)(a + 1u), c->h, I8080_CYC_MEMW); } break;
    case 0x2A: { uint16_t a = fetch16(c, b);                 /* LHLD */
                 c->l = bus_rd(c, b, a, I8080_CYC_MEMR);
                 c->h = bus_rd(c, b, (uint16_t)(a + 1u), I8080_CYC_MEMR); } break;
    case 0x32: { uint16_t a = fetch16(c, b);                 /* STA  */
                 bus_wr(c, b, a, c->a, I8080_CYC_MEMW); } break;
    case 0x3A: { uint16_t a = fetch16(c, b);                 /* LDA  */
                 c->a = bus_rd(c, b, a, I8080_CYC_MEMR); } break;

    /* --- incrementos de 16 bits --- */
    case 0x03: set_bc(c, (uint16_t)(get_bc(c) + 1u)); break;
    case 0x13: set_de(c, (uint16_t)(get_de(c) + 1u)); break;
    case 0x23: set_hl(c, (uint16_t)(get_hl(c) + 1u)); break;
    case 0x33: c->sp = (uint16_t)(c->sp + 1u); break;
    case 0x0B: set_bc(c, (uint16_t)(get_bc(c) - 1u)); break;
    case 0x1B: set_de(c, (uint16_t)(get_de(c) - 1u)); break;
    case 0x2B: set_hl(c, (uint16_t)(get_hl(c) - 1u)); break;
    case 0x3B: c->sp = (uint16_t)(c->sp - 1u); break;

    case 0x09: alu_dad(c, get_bc(c)); break;
    case 0x19: alu_dad(c, get_de(c)); break;
    case 0x29: alu_dad(c, get_hl(c)); break;
    case 0x39: alu_dad(c, c->sp); break;

    /* --- rotaciones --- */
    case 0x07: { bool cy = (c->a & 0x80u) != 0u;             /* RLC */
                 c->a = (uint8_t)((c->a << 1) | (cy ? 1u : 0u));
                 set_flag(c, I8080_F_C, cy); } break;
    case 0x0F: { bool cy = (c->a & 0x01u) != 0u;             /* RRC */
                 c->a = (uint8_t)((c->a >> 1) | (cy ? 0x80u : 0u));
                 set_flag(c, I8080_F_C, cy); } break;
    case 0x17: { bool old = get_flag(c, I8080_F_C);          /* RAL */
                 bool cy = (c->a & 0x80u) != 0u;
                 c->a = (uint8_t)((c->a << 1) | (old ? 1u : 0u));
                 set_flag(c, I8080_F_C, cy); } break;
    case 0x1F: { bool old = get_flag(c, I8080_F_C);          /* RAR */
                 bool cy = (c->a & 0x01u) != 0u;
                 c->a = (uint8_t)((c->a >> 1) | (old ? 0x80u : 0u));
                 set_flag(c, I8080_F_C, cy); } break;

    /* --- varios de un byte --- */
    case 0x27: alu_daa(c); break;                            /* DAA */
    case 0x2F: c->a = (uint8_t)~c->a; break;                 /* CMA */
    case 0x37: set_flag(c, I8080_F_C, true); break;          /* STC */
    case 0x3F: set_flag(c, I8080_F_C, !get_flag(c, I8080_F_C)); break; /* CMC */
    case 0xEB: { uint16_t t = get_hl(c);                     /* XCHG */
                 set_hl(c, get_de(c)); set_de(c, t); } break;
    case 0xE3: { uint16_t t = pop16(c, b);                   /* XTHL */
                 push16(c, b, get_hl(c)); set_hl(c, t); } break;
    case 0xE9: c->pc = get_hl(c); break;                     /* PCHL */
    case 0xF9: c->sp = get_hl(c); break;                     /* SPHL */
    case 0xF3: c->inte = false; c->int_delay = 0; break;     /* DI */
    case 0xFB: c->inte = true;  c->int_delay = 1; break;     /* EI */

    /* --- pila --- */
    case 0xC1: set_bc(c, pop16(c, b)); break;
    case 0xD1: set_de(c, pop16(c, b)); break;
    case 0xE1: set_hl(c, pop16(c, b)); break;
    case 0xF1: { uint16_t v = pop16(c, b);                   /* POP PSW */
                 c->a = (uint8_t)(v >> 8);
                 c->f = (uint8_t)((v & I8080_F_MASK) | I8080_F_ONE); } break;
    case 0xC5: push16(c, b, get_bc(c)); break;
    case 0xD5: push16(c, b, get_de(c)); break;
    case 0xE5: push16(c, b, get_hl(c)); break;
    case 0xF5: push16(c, b, (uint16_t)((c->a << 8) |
                     ((c->f & I8080_F_MASK) | I8080_F_ONE))); break;

    /* --- entrada/salida --- */
    case 0xDB: c->a = bus_in(c, b, fetch8(c, b)); break;     /* IN  */
    case 0xD3: { uint8_t p = fetch8(c, b);                   /* OUT */
                 bus_out(c, b, p, c->a); } break;

    /* --- HLT --- */
    case 0x76: c->halted = true; break;

    /* --- saltos, llamadas y retornos incondicionales --- */
    case 0xC3: case 0xCB: c->pc = fetch16(c, b); break;      /* JMP (CB no doc.) */
    case 0xCD: case 0xDD: case 0xED: case 0xFD: {            /* CALL (3 no doc.) */
                 uint16_t a = fetch16(c, b);
                 push16(c, b, c->pc);
                 c->pc = a; } break;
    case 0xC9: case 0xD9: c->pc = pop16(c, b); break;        /* RET (D9 no doc.) */

    /* --- NOP y sus siete variantes no documentadas --- */
    case 0x00: case 0x08: case 0x10: case 0x18:
    case 0x20: case 0x28: case 0x30: case 0x38:
        break;

    default:
        /* --- bloques regulares --- */
        if (op >= 0x40u && op <= 0x7Fu) {                     /* MOV (0x76 ya tratado) */
            unsigned dst = (unsigned)((op >> 3) & 7u);
            unsigned src = (unsigned)(op & 7u);
            reg_set(c, b, dst, reg_get(c, b, src));
        } else if (op >= 0x80u && op <= 0xBFu) {              /* ALU r */
            unsigned aop = (unsigned)((op >> 3) & 7u);
            alu_op(c, aop, reg_get(c, b, (unsigned)(op & 7u)));
        } else if ((op & 0xC7u) == 0x04u) {                   /* INR r */
            alu_inr(c, b, (unsigned)((op >> 3) & 7u));
        } else if ((op & 0xC7u) == 0x05u) {                   /* DCR r */
            alu_dcr(c, b, (unsigned)((op >> 3) & 7u));
        } else if ((op & 0xC7u) == 0x06u) {                   /* MVI r */
            uint8_t imm = fetch8(c, b);
            reg_set(c, b, (unsigned)((op >> 3) & 7u), imm);
        } else if ((op & 0xC7u) == 0xC6u) {                   /* ALU inmediato */
            alu_op(c, (unsigned)((op >> 3) & 7u), fetch8(c, b));
        } else if ((op & 0xC7u) == 0xC7u) {                   /* RST n */
            push16(c, b, c->pc);
            c->pc = (uint16_t)(op & 0x38u);
        } else if ((op & 0xC7u) == 0xC2u) {                   /* Jcc */
            uint16_t a = fetch16(c, b);
            if (cond_true(c, (unsigned)((op >> 3) & 7u))) c->pc = a;
            /* Jcc consume 10 ciclos T se tome o no. */
        } else if ((op & 0xC7u) == 0xC4u) {                   /* Ccc */
            uint16_t a = fetch16(c, b);
            if (cond_true(c, (unsigned)((op >> 3) & 7u))) {
                push16(c, b, c->pc);
                c->pc = a;
                cyc = 17u;
            }
        } else if ((op & 0xC7u) == 0xC0u) {                   /* Rcc */
            if (cond_true(c, (unsigned)((op >> 3) & 7u))) {
                c->pc = pop16(c, b);
                cyc = 11u;
            }
        }
        break;
    }

    return cyc;
}

unsigned i8080_step(i8080_t *cpu, const i8080_bus_t *bus)
{
    unsigned cyc;

    cpu->mc_count = 0;

    /* Atender interrupción: sólo si INTE está activo y ya pasó la
     * instrucción de gracia que impone EI. El biestable se limpia al
     * reconocerla, igual que en el 8080 real. */
    if (cpu->int_pending && cpu->inte && cpu->int_delay == 0u) {
        uint8_t vec = cpu->int_vector;
        cpu->int_pending = false;
        cpu->inte = false;

        /* Ciclo INTA: la palabra de estado depende de si estábamos en HLT. */
        mc_log(cpu, cpu->halted ? I8080_CYC_INTA_HALT : I8080_CYC_INTA,
               cpu->pc, vec);
        cpu->halted = false;

        cyc = execute(cpu, bus, vec);
        cpu->cycles += cyc;
        return cyc;
    }

    /* En HLT la CPU sigue emitiendo ciclos de reconocimiento de parada. */
    if (cpu->halted) {
        mc_log(cpu, I8080_CYC_HALTA, cpu->pc, 0x00u);
        cpu->cycles += 4u;
        return 4u;
    }

    {
        uint8_t op = bus_rd(cpu, bus, cpu->pc, I8080_CYC_FETCH);
        uint8_t delay_before = cpu->int_delay;

        cpu->pc = (uint16_t)(cpu->pc + 1u);
        cyc = execute(cpu, bus, op);

        /* Consumir la instrucción de gracia de EI — pero sólo si esta
         * instrucción no acaba de armarla ella misma (EI), y sin resucitar
         * el retardo que DI haya anulado. */
        if (delay_before > 0u && cpu->int_delay == delay_before) {
            cpu->int_delay--;
        }
    }

    cpu->cycles += cyc;
    return cyc;
}
