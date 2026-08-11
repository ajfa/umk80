/* criterion4.c — acceptance criterion 4.
 *
 *   In single-step mode (РБ/ШГ latched, КМ/ЦК released), each press of ШГ
 *   advances exactly one instruction; with КМ/ЦК latched, exactly one
 *   machine cycle.
 *
 * Beyond counting presses this also checks:
 *   - that with РБ/ШГ latched the machine really is frozen;
 *   - that in cycle mode the state changes only when the LAST machine cycle
 *     of the instruction is stepped, not before;
 *   - that the АДРЕС/ДАННЫЕ/СОСТОЯНИЕ panel follows the bus cycle by cycle,
 *     with the correct status word on each.
 */

#include "umk80/umk80.h"

#include <stdio.h>

/* Test program, chosen so there are instructions of 1, 2 and 3 machine
 * cycles plus one observable memory write. */
static const uint8_t PROG[] = {
    0x3E, 0xAA,             /* 0800  MVI A,0AAH   -> 2 cycles: M1 + read   */
    0x06, 0x55,             /* 0802  MVI B,55H    -> 2                      */
    0x21, 0x00, 0x09,       /* 0804  LXI H,0900H  -> 3: M1 + 2 reads        */
    0x77,                   /* 0807  MOV M,A      -> 2: M1 + write          */
    0x3C,                   /* 0808  INR A        -> 1: M1 only             */
    0xC3, 0x00, 0x08        /* 0809  JMP 0800H    -> 3                      */
};

static const uint16_t PC_SEQ[6] = { 0x0802, 0x0804, 0x0807, 0x0808, 0x0809, 0x0800 };
static const unsigned MC_SEQ[6] = { 2, 2, 3, 2, 1, 3 };
static const char *const NAME[6] = {
    "MVI A,0AAH", "MVI B,55H", "LXI H,0900H", "MOV M,A", "INR A", "JMP 0800H"
};

static const char *status_name(uint8_t s)
{
    switch (s) {
        case I8080_CYC_FETCH:  return "M1 (opcode fetch)";
        case I8080_CYC_MEMR:   return "memory read";
        case I8080_CYC_MEMW:   return "memory write";
        case I8080_CYC_STACKR: return "stack read";
        case I8080_CYC_STACKW: return "stack write";
        case I8080_CYC_INPR:   return "input";
        case I8080_CYC_OUTW:   return "output";
        case I8080_CYC_HALTA:  return "halt acknowledge";
        default:               return "?";
    }
}

static void setup(umk_machine_t *m)
{
    umk_init(m, UMK_REV2);
    umk_load_ram(m, 0x0800u, PROG, sizeof PROG);
    m->cpu.pc = 0x0800u;
    umk_poke(m, 0x0900u, 0x00u);
}

int main(void)
{
    static umk_machine_t m;
    int ok = 1;
    unsigned i, c;

    printf("=== Criterion 4: stepping by instruction and by machine cycle ===\n");

    /* ---------------------------------------------------------------------
     * a) РБ/ШГ latched freezes the machine.
     * ------------------------------------------------------------------- */
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, false);
    {
        uint64_t before = m.cpu.cycles;
        umk_run_cycles(&m, 1000000u);
        if (m.cpu.cycles != before || m.cpu.pc != 0x0800u) {
            printf("FAIL: with РБ/ШГ latched the machine keeps running\n");
            ok = 0;
        } else {
            printf("\nOK: with РБ/ШГ latched the machine is halted\n");
        }
    }

    /* ---------------------------------------------------------------------
     * b) One press of ШГ = one instruction.
     * ------------------------------------------------------------------- */
    printf("\n-- РБ/ШГ latched, КМ/ЦК released: one press = one instruction --\n");
    for (i = 0; i < 6u; i++) {
        umk_press_step(&m);
        printf("  ШГ %u: %-12s  PC=%04X  (expected %04X)\n",
               i + 1u, NAME[i], m.cpu.pc, PC_SEQ[i]);
        if (m.cpu.pc != PC_SEQ[i]) {
            printf("     FAIL: the PC did not advance exactly one instruction\n");
            ok = 0;
        }
    }
    if (m.cpu.a != 0xABu || m.cpu.b != 0x55u) {
        printf("  FAIL: unexpected registers A=%02X B=%02X\n", m.cpu.a, m.cpu.b);
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0xAAu) {
        printf("  FAIL: MOV M,A did not write to 0900H\n");
        ok = 0;
    }

    /* ---------------------------------------------------------------------
     * c) With КМ/ЦК latched, one press = one machine cycle.
     * ------------------------------------------------------------------- */
    printf("\n-- РБ/ШГ and КМ/ЦК latched: one press = one machine cycle --\n");
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, true);

    for (i = 0; i < 6u; i++) {
        uint16_t pc_before = m.cpu.pc;
        unsigned presses = 0;
        printf("  %-12s (%u cycles expected)\n", NAME[i], MC_SEQ[i]);

        for (c = 0; c < 12u; c++) {
            bool done;
            uint8_t mem_before = umk_peek(&m, 0x0900u);

            done = umk_step_machine_cycle(&m);
            presses++;

            printf("     cycle %u: АДРЕС=%04X ДАННЫЕ=%02X СОСТОЯНИЕ=%02X  %-20s%s\n",
                   presses, m.panel.address, m.panel.data, m.panel.status,
                   status_name(m.panel.status), done ? "  <- last" : "");

            /* Nothing may have changed before the last cycle. */
            if (!done) {
                if (m.cpu.pc != pc_before) {
                    printf("     FAIL: the PC changed before the last cycle\n");
                    ok = 0;
                }
                if (umk_peek(&m, 0x0900u) != mem_before) {
                    printf("     FAIL: memory changed before the last cycle\n");
                    ok = 0;
                }
            }
            if (done) break;
        }

        if (presses != MC_SEQ[i]) {
            printf("     FAIL: it took %u presses, not %u\n", presses, MC_SEQ[i]);
            ok = 0;
        }
        if (m.cpu.pc != PC_SEQ[i]) {
            printf("     FAIL: PC=%04X on completion, expected %04X\n",
                   m.cpu.pc, PC_SEQ[i]);
            ok = 0;
        }
    }

    /* The first cycle of every instruction must be an opcode fetch (M1) at
     * the instruction's own address; and in MOV M,A the second cycle must be
     * a write to 0900H carrying AAh. Checked again, directly. */
    printf("\n-- status word and bus, cycle by cycle, in MOV M,A --\n");
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, true);
    for (i = 0; i < 3u; i++)                    /* skip the first three */
        while (!umk_step_machine_cycle(&m)) { }

    umk_step_machine_cycle(&m);                 /* cycle 1 of MOV M,A */
    printf("  cycle 1: АДРЕС=%04X СОСТОЯНИЕ=%02X %s\n",
           m.panel.address, m.panel.status, status_name(m.panel.status));
    if (m.panel.status != I8080_CYC_FETCH || m.panel.address != 0x0807u) {
        printf("     FAIL: expected an M1 fetch at 0807H\n");
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0x00u) {
        printf("     FAIL: the write happened during the fetch cycle\n");
        ok = 0;
    }

    umk_step_machine_cycle(&m);                 /* cycle 2: the write */
    printf("  cycle 2: АДРЕС=%04X ДАННЫЕ=%02X СОСТОЯНИЕ=%02X %s\n",
           m.panel.address, m.panel.data, m.panel.status, status_name(m.panel.status));
    if (m.panel.status != I8080_CYC_MEMW || m.panel.address != 0x0900u ||
        m.panel.data != 0xAAu) {
        printf("     FAIL: expected a write of AAh to 0900H\n");
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0xAAu) {
        printf("     FAIL: the write did not materialise on its own cycle\n");
        ok = 0;
    }

    /* ---------------------------------------------------------------------
     * d) Releasing РБ/ШГ returns the machine to normal running.
     * ------------------------------------------------------------------- */
    umk_set_switch(&m, UMK_SW_STEP, false);
    {
        uint64_t before = m.cpu.cycles;
        umk_run_cycles(&m, 10000u);
        if (m.cpu.cycles <= before) {
            printf("\nFAIL: releasing РБ/ШГ does not resume the machine\n");
            ok = 0;
        } else {
            printf("\nOK: releasing РБ/ШГ resumes the machine\n");
        }
    }

    printf("\n===== %s =====\n", ok ? "CRITERION 4 MET" : "CRITERION 4 NOT MET");
    return ok ? 0 : 1;
}
