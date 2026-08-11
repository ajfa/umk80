/* i8080.h — Intel 8080 / КР580ВМ80А CPU core.
 *
 * Freestanding: only <stdint.h>, <stddef.h> and <stdbool.h>. No dynamic
 * allocation, no global state, no I/O.
 *
 * `i8080_t` is a plain POD: it holds no pointers. Saving and restoring the
 * CPU state is a struct copy. Access to the outside world goes through
 * `i8080_bus_t`, which the caller passes on every step and which is NOT part
 * of the state.
 */
#ifndef UMK80_I8080_H
#define UMK80_I8080_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- 8080 status word ----------------------------------------------------
 *
 * During SYNC the КР580ВМ80А puts a status word on D0..D7. On the УМК-80 it
 * is latched by register D4 and shown on the «СОСТОЯНИЕ» LED row. The panel
 * legends, from bit 7 down to bit 0, are:
 *
 *   MEMR  INP  M1  OUT  HLTA  STACK  WO  INTA
 *
 * Mind WO (bit 1): it is active low. WO = 1 means a read; WO = 0 means a
 * write or an output.
 */
#define I8080_ST_INTA   0x01u
#define I8080_ST_WO     0x02u
#define I8080_ST_STACK  0x04u
#define I8080_ST_HLTA   0x08u
#define I8080_ST_OUT    0x10u
#define I8080_ST_M1     0x20u
#define I8080_ST_INP    0x40u
#define I8080_ST_MEMR   0x80u

/* The ten status words an 8080 can emit. */
#define I8080_CYC_FETCH     (I8080_ST_MEMR | I8080_ST_M1 | I8080_ST_WO)  /* 0xA2 */
#define I8080_CYC_MEMR      (I8080_ST_MEMR | I8080_ST_WO)                /* 0x82 */
#define I8080_CYC_MEMW      (0x00u)                                      /* 0x00 */
#define I8080_CYC_STACKR    (I8080_ST_MEMR | I8080_ST_STACK | I8080_ST_WO)/* 0x86 */
#define I8080_CYC_STACKW    (I8080_ST_STACK)                             /* 0x04 */
#define I8080_CYC_INPR      (I8080_ST_INP | I8080_ST_WO)                 /* 0x42 */
#define I8080_CYC_OUTW      (I8080_ST_OUT)                               /* 0x10 */
#define I8080_CYC_INTA      (I8080_ST_INTA | I8080_ST_M1 | I8080_ST_WO)  /* 0x23 */
#define I8080_CYC_HALTA     (I8080_ST_MEMR | I8080_ST_HLTA | I8080_ST_WO)/* 0x8A */
#define I8080_CYC_INTA_HALT (I8080_ST_INTA | I8080_ST_HLTA | \
                             I8080_ST_M1 | I8080_ST_WO)                  /* 0x2B */

/* --- Condition flags (PSW) -----------------------------------------------
 *
 * On the 8080 bit 1 always reads as 1 and bits 3 and 5 always read as 0.
 * That invariant is preserved in the register, in PUSH PSW and in POP PSW.
 */
#define I8080_F_C   0x01u
#define I8080_F_ONE 0x02u   /* always 1 */
#define I8080_F_P   0x04u
#define I8080_F_AC  0x10u
#define I8080_F_Z   0x40u
#define I8080_F_S   0x80u

/* Mask of the bits that actually exist. */
#define I8080_F_MASK (I8080_F_C | I8080_F_P | I8080_F_AC | I8080_F_Z | I8080_F_S)

/* --- CPU state ------------------------------------------------------------ */
typedef struct {
    uint8_t  a, f;
    uint8_t  b, c, d, e, h, l;
    uint16_t sp, pc;

    bool     inte;          /* interrupt enable flip-flop */
    bool     halted;        /* inside HLT */
    bool     int_pending;   /* an interrupt request is waiting */
    uint8_t  int_vector;    /* opcode the external logic jams in during INTA */
    uint8_t  int_delay;     /* after EI one more instruction executes before an
                             * interrupt can be recognised; DI cancels it */

    uint64_t cycles;        /* T states consumed since reset */

    /* Machine-cycle trace of the last instruction executed. Feeds the LED
     * panel and the machine-cycle single-step mode (КМ/ЦК). An 8080 never
     * exceeds 5 machine cycles per instruction. */
    uint8_t  mc_count;
    uint8_t  mc_status[6];
    uint16_t mc_addr[6];
    uint8_t  mc_data[6];
} i8080_t;

/* --- Interface to the outside world --------------------------------------
 *
 * `status` is the I8080_CYC_* word of the machine cycle in progress, so the
 * system can tell an opcode fetch from a data read or a stack access without
 * having to reconstruct it.
 */
typedef struct {
    uint8_t (*read)(void *ud, uint16_t addr, uint8_t status);
    void    (*write)(void *ud, uint16_t addr, uint8_t val, uint8_t status);
    uint8_t (*in)(void *ud, uint8_t port);
    void    (*out)(void *ud, uint8_t port, uint8_t val);
    void    *ud;
} i8080_bus_t;

/* --- API ------------------------------------------------------------------ */

/* Puts the CPU in its post-RESET state: PC = 0, INTE = 0, HLTA = 0. On real
 * hardware the registers are undefined after reset; here they are zeroed so
 * that runs are reproducible. */
void i8080_reset(i8080_t *cpu);

/* Executes one complete instruction (or services a pending interrupt).
 * Returns the T states consumed and adds them to cpu->cycles. Leaves the
 * machine-cycle trace in cpu->mc_*. */
unsigned i8080_step(i8080_t *cpu, const i8080_bus_t *bus);

/* Requests an interrupt. `vector` is the byte the external logic will place
 * on the bus during the INTA cycle; on the УМК-80 the ПР button generates
 * RST 7, i.e. 0xFF. The request is serviced on the next i8080_step() if INTE
 * is set. */
void i8080_interrupt(i8080_t *cpu, uint8_t vector);

/* T states for an opcode, not counting the taken path of conditional jumps
 * and calls (for those it returns the NOT-taken case). Useful for the
 * disassembler and for timing budgets. */
unsigned i8080_opcode_cycles(uint8_t opcode);

/* Length of an opcode in bytes: 1, 2 or 3. */
unsigned i8080_opcode_length(uint8_t opcode);

/* True if the opcode is undocumented by Intel (the seven alternative NOPs,
 * the alternative JMP, the alternative RET and the three alternative CALLs). */
bool i8080_opcode_undocumented(uint8_t opcode);

#ifdef __cplusplus
}
#endif
#endif /* UMK80_I8080_H */
