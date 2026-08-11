/* umk80.h — УМК-80 emulation core (ВЭФ, РР3.059.004).
 *
 * Public API of the core. Freestanding: only <stdint.h>, <stddef.h> and
 * <stdbool.h>. No dynamic allocation, no global state, no console or GUI
 * I/O. The whole machine lives in one `umk_machine_t` owned by the caller,
 * and it holds no pointers, so saving and restoring the entire state is a
 * struct copy.
 *
 * Documentary references (docs/SOURCES.md):
 *   ПС  = РР3.059.004 ПС, the equipment manual
 *   МОН = Р.Р.00004-01 12 01-1, Системный монитор, program text
 */
#ifndef UMK80_UMK80_H
#define UMK80_UMK80_H

#include "umk80/i8080.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Machine profiles ----------------------------------------------------
 *
 * The documentation this was built from describes revision 2 (ПС p. 8, with
 * the handwritten correction of doc. РР1323-87, 10.09.89): a single RAM chip
 * КР537РУ8А of 2 K×8 and a single ROM chip К573РФ2 of 2 K×8. The monitor
 * itself confirms it with RAMEND = 1000H.
 *
 * Revision 1 is the one described by eax.me: 2 KB of ROM in two К573РФ1 and
 * 1 KB of RAM. For the monitor to work on that revision the RAM has to be
 * mirrored across 0800h-0FFFh, because the monitor uses 0FFAh.
 * See UNKNOWNS.md §1.
 */
typedef enum {
    UMK_REV1 = 1,   /* ROM 2 KB + RAM 1 KB mirrored */
    UMK_REV2 = 2    /* ROM 2 KB + RAM 2 KB (as documented) */
} umk_rev_t;

#define UMK_ROM_MAX     2048u
#define UMK_RAM_MAX     2048u
#define UMK_ROM_BASE    0x0000u
#define UMK_RAM_BASE    0x0800u
#define UMK_DIGITS      6u
#define UMK_SEGMENTS    8u          /* A..G plus the decimal point */
#define UMK_KEY_ROWS    4u
#define UMK_KEY_COLS    6u

/* Ports, exactly as the monitor itself declares them (МОН sheet −4−). */
#define UMK_PORT_A      0xF8u       /* ПОРТ АДРЕСА:    digit select       */
#define UMK_PORT_B      0xF9u       /* ПОРТ ДАННЫХ:    segment mask       */
#define UMK_PORT_C      0xFAu       /* ПОРТ СОСТОЯНИЯ: keyboard rows      */
#define UMK_PORT_CTRL   0xFBu       /* ПОРТ УПРАВЛ. БИС: ВВ55 control     */
#define UMK_PORT_DBG    0xFCu       /* ПОРТ ПОШАГ. РЕЖ.: single-step mode */

/* PORTC bits carrying the four keyboard rows: the 74h mask used by CILOOP
 * (МОН sheet −30−) and by the key-release wait loop. */
#define UMK_KEY_ROW_MASK 0x74u

/* --- КР580ВВ55А (8255) --------------------------------------------------- */
typedef struct {
    uint8_t ctrl;        /* last mode word written (bit 7 = 1) */
    uint8_t out_a;       /* port A latch */
    uint8_t out_b;       /* port B latch */
    uint8_t out_c;       /* port C latch (whichever half is an output) */
    bool    a_is_out;
    bool    b_is_out;
    bool    c_lo_is_out;
    bool    c_hi_is_out;
} umk_ppi_t;

/* --- Seven-segment displays ----------------------------------------------
 *
 * These are not modelled as six digits with independent state. Instead the
 * energy of each (digit, segment) pair is integrated over simulated time with
 * exponential decay. A program that changes the segment mask without blanking
 * the displays first produces the characteristic ghosting by construction,
 * with no code simulating it.
 */
typedef struct {
    uint32_t energy[UMK_DIGITS][UMK_SEGMENTS];  /* fixed point, scale 1/256 */
    uint32_t energy_digit[UMK_DIGITS];          /* time that digit was selected */
    uint32_t total;                             /* same scale; normaliser */
    uint64_t integrated_to;                     /* T state integrated up to */
    uint32_t tau_cycles;                        /* persistence constant */
    uint32_t decay_acc;                         /* remainder between decay steps */
    bool     bit0_is_left;                      /* digit orientation */
} umk_display_t;

/* --- LED panel -----------------------------------------------------------
 *
 * АДРЕС (16), ДАННЫЕ (8) and СОСТОЯНИЕ (8) are hardware latches that follow
 * the last machine cycle executed.
 *
 * The three supply LEDs are FAULT indicators: lit means that rail is
 * MISSING. See UNKNOWNS.md §8.
 */
typedef struct {
    uint16_t address;
    uint8_t  data;
    uint8_t  status;     /* I8080_ST_* word of the last machine cycle */
    bool     fault_p5;   /* true = +5 V missing  -> LED lit */
    bool     fault_m5;   /* true = -5 V missing  */
    bool     fault_p12;  /* true = +12 V missing */
} umk_panel_t;

/* --- Single-step mode ----------------------------------------------------
 *
 * РБ/ШГ (S3) and КМ/ЦК (S4) are LATCHING switches: they stay pressed
 * (ПС p. 28). ШГ (S2), СБ (S1) and ПР (S5) are momentary buttons.
 *
 * How machine-cycle stepping is modelled: on the first press the instruction
 * is executed "dry" to learn its cycle sequence and then rolled back
 * completely (registers, RAM, the ВВ55 and the displays). Subsequent presses
 * show each cycle of that sequence on the panel, and only pressing the LAST
 * one executes it for real. That way machine state changes exactly when it
 * changes on the real equipment, where the CPU is frozen by READY between
 * cycles.
 *
 * What is still not faithful in detail: in an instruction with several writes
 * (CALL, PUSH) they all land together on the last cycle instead of each on
 * its own. That is not observable from the panel, which is the only thing the
 * machine shows in this mode. See UNKNOWNS.md §7.
 */
typedef struct {
    bool    latch_step;      /* РБ/ШГ latched: single-step mode active   */
    bool    latch_cycle;     /* КМ/ЦК latched: each step is one cycle    */
    bool    waiting;         /* CPU halted, waiting for ШГ               */
    uint8_t dbg_port;        /* last value written to 0FCh               */

    /* Machine-cycle sequence of the instruction in progress, obtained by dry
     * execution. */
    uint8_t  mc_index;
    uint8_t  mc_total;
    uint8_t  mc_status[6];
    uint16_t mc_addr[6];
    uint8_t  mc_data[6];
} umk_step_t;

/* --- Keyboard ------------------------------------------------------------
 *
 * A 6 column × 4 row matrix = 24 keys (ПС p. 34: «24 клавиш, из них
 * 8 клавиш директивные, а 16 — информационные»). The columns are the same
 * six PORTA lines that select the digit; the rows are bits 2, 4, 5 and 6 of
 * PORTC, idling high.
 *
 * Which physical key sits at each intersection was derived from the monitor's
 * CONV routine — see UNKNOWNS.md §4.
 */
typedef struct {
    bool pressed[UMK_KEY_COLS][UMK_KEY_ROWS];
} umk_keyboard_t;

/* --- The machine ---------------------------------------------------------- */
typedef struct {
    uint32_t      magic;
    uint16_t      version;

    umk_rev_t     rev;
    uint16_t      rom_size;
    uint16_t      ram_size;
    uint32_t      clock_hz;

    uint8_t       rom[UMK_ROM_MAX];
    uint8_t       ram[UMK_RAM_MAX];
    bool          rom_present;

    i8080_t       cpu;
    umk_ppi_t     ppi;
    umk_display_t display;
    umk_panel_t   panel;
    umk_step_t    step;
    umk_keyboard_t kbd;

    /* When the instruction in progress started; the display integrator uses
     * it to place the OUT write inside the instruction. */
    uint64_t      step_start_cycles;

    /* Count of writes to undecoded ports: useful for spotting programs that
     * talk to hardware this machine does not have. */
    uint32_t      unmapped_io_writes;

    /* Journal of RAM writes during the dry run of machine-cycle stepping.
     * Never more than one entry per machine cycle is needed. */
    bool          trial;
    uint8_t       trial_n;
    uint16_t      trial_idx[8];
    uint8_t       trial_old[8];
} umk_machine_t;

/* --- Lifecycle ------------------------------------------------------------ */

/* Leaves the machine in its power-on state: RAM zeroed, ROM unloaded, CPU in
 * reset, ВВ55 unprogrammed, no supply faults. */
void umk_init(umk_machine_t *m, umk_rev_t rev);

/* Loads an image into ROM starting at `offset` (0 for the monitor, 0x400 for
 * the ПРОГРАММАТОР). Returns false if it does not fit. */
bool umk_load_rom(umk_machine_t *m, uint16_t offset,
                  const uint8_t *data, size_t len);

/* Loads bytes into memory through the same path the CPU would see (address
 * decoding applies: writing to ROM has no effect). */
void umk_load_ram(umk_machine_t *m, uint16_t addr,
                  const uint8_t *data, size_t len);

/* The СБ button (S1). */
void umk_reset(umk_machine_t *m);

/* The ПР button (S5): interrupt request. On the УМК-80 the vector is RST 7
 * (opcode 0FFh), which the monitor services at 0038h to save the state of
 * every register into RAM. */
void umk_interrupt(umk_machine_t *m);

/* --- Execution ------------------------------------------------------------ */

/* Runs instructions until at least `cycles` T states have been consumed.
 * Returns the cycles actually consumed (it may overshoot by the last
 * instruction). Does nothing while the machine is halted waiting for ШГ. */
uint64_t umk_run_cycles(umk_machine_t *m, uint64_t cycles);

/* Executes exactly one instruction. Returns its T states. */
unsigned umk_step_instruction(umk_machine_t *m);

/* Advances one machine cycle (a press of ШГ with КМ/ЦК latched).
 * Returns true if this cycle completed the instruction. */
bool umk_step_machine_cycle(umk_machine_t *m);

/* --- Controls ------------------------------------------------------------- */

typedef enum {
    UMK_SW_STEP  = 0,   /* РБ/ШГ (S3) */
    UMK_SW_CYCLE = 1    /* КМ/ЦК (S4), also documented as ММ/ЦИ */
} umk_switch_t;

void umk_set_switch(umk_machine_t *m, umk_switch_t sw, bool latched);
bool umk_get_switch(const umk_machine_t *m, umk_switch_t sw);

/* The ШГ button (S2). No effect unless РБ/ШГ is latched. */
void umk_press_step(umk_machine_t *m);

/* A key of the matrix. `col` in 0..5, `row` in 0..3. */
void umk_set_key(umk_machine_t *m, unsigned col, unsigned row, bool down);
void umk_release_all_keys(umk_machine_t *m);

/* Supply faults, so the LEDs can be exercised. */
void umk_set_power_fault(umk_machine_t *m, bool p5, bool m5, bool p12);

/* --- Display -------------------------------------------------------------- */

/* Digit orientation. Settled in UNKNOWNS.md §3: bit 0 of PORTA is the
 * leftmost display. */
typedef enum {
    UMK_DIGIT_BIT0_LEFT  = 0,   /* the default, and what the documentation says */
    UMK_DIGIT_BIT0_RIGHT = 1
} umk_digit_order_t;

void umk_display_set_digit_order(umk_machine_t *m, umk_digit_order_t order);

/* Display persistence constant, in T states. Defaults to about 20 ms at the
 * configured clock. See UNKNOWNS.md §5. */
void umk_display_set_persistence(umk_machine_t *m, uint32_t tau_cycles);

/* ABSOLUTE brightness of each segment, 0..255, indexed by panel position
 * (0 = leftmost). 255 would be a segment lit continuously; on a six-digit
 * multiplexed display the real maximum is around 255/6. This is what the
 * frontend should draw. */
void umk_display_intensity(const umk_machine_t *m,
                           uint8_t out[UMK_DIGITS][UMK_SEGMENTS]);

/* Brightness RELATIVE TO ITS OWN DIGIT, 0..255: 255 means "that segment was
 * lit whenever its digit was selected". This is the quantity that tells which
 * character each display shows, and therefore the one that reveals ghosting:
 * a clean digit gives 255 or 0 on every segment, a ghosted one gives
 * intermediate values on the intruding letter's segments. */
void umk_display_relative(const umk_machine_t *m,
                          uint8_t out[UMK_DIGITS][UMK_SEGMENTS]);

/* Time-averaged segment mask of each digit: bit s set if that segment's
 * relative brightness exceeds `threshold` (0..255). With a low threshold
 * (say 25, i.e. 10 %) the mask also picks up the intruding letter, which is
 * how ghosting is detected in the automated test. */
void umk_display_pattern(const umk_machine_t *m, uint8_t threshold,
                         uint8_t out[UMK_DIGITS]);

/* Resets the integration window, to average over a specific interval in
 * automated tests. */
void umk_display_clear_accumulator(umk_machine_t *m);

/* --- State ---------------------------------------------------------------- */

/* Size of the state dump. */
size_t umk_state_size(void);

/* Saves / restores the entire machine state. `buf` must hold at least
 * umk_state_size() bytes. Restoring checks the magic number and the version,
 * and returns false if they do not match. */
bool umk_state_save(const umk_machine_t *m, void *buf, size_t len);
bool umk_state_load(umk_machine_t *m, const void *buf, size_t len);

/* --- Direct access (debugger, tests) -------------------------------------- */

/* Read and write through the CPU's path, incomplete address decoding
 * included. `umk_peek` disturbs neither the panel nor the display. */
uint8_t umk_peek(const umk_machine_t *m, uint16_t addr);
void    umk_poke(umk_machine_t *m, uint16_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif
#endif /* UMK80_UMK80_H */
