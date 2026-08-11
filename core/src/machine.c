/* machine.c — la máquina УМК-80 completa: memoria, КР580ВВ55А, indicadores,
 * teclado, panel de LEDs y modo paso a paso.
 *
 * Independiente: sólo las cabeceras del propio núcleo. Sin libc.
 */

#include "umk80/umk80.h"

#define UMK_MAGIC   0x554D4B38u   /* 'UMK8' */
#define UMK_VERSION 1u

/* --- utilidades sin libc -------------------------------------------------- */

static void zero(void *p, size_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = 0u;
}

static void copy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
}

/* --- decodificación de direcciones ---------------------------------------
 *
 * La zona decodificada es 0000h-0FFFh: ПЗУ en 0000h-07FFh y ОЗУ en
 * 0800h-0FFFh. Fuera de ella se devuelve FFh (bus sin excitar). En la
 * revisión 1, con sólo 1 KB de ОЗУ, el chip se repite en 0C00h-0FFFh, que
 * es lo que hace que el búfer del monitor en 0FFAh caiga sobre memoria
 * real. Ver DESCONOCIDOS.md §1: el detalle del decodificador (D15, D16)
 * está inferido, no leído del esquema.
 */
static bool decode(const umk_machine_t *m, uint16_t addr,
                   bool *is_rom, uint16_t *index)
{
    if (addr >= (UMK_RAM_BASE + UMK_RAM_MAX)) return false;

    if (addr < UMK_RAM_BASE) {
        *is_rom = true;
        *index  = (uint16_t)(addr & (uint16_t)(m->rom_size - 1u));
    } else {
        *is_rom = false;
        *index  = (uint16_t)((uint16_t)(addr - UMK_RAM_BASE) &
                             (uint16_t)(m->ram_size - 1u));
    }
    return true;
}

uint8_t umk_peek(const umk_machine_t *m, uint16_t addr)
{
    bool is_rom; uint16_t idx;
    if (!decode(m, addr, &is_rom, &idx)) return 0xFFu;
    return is_rom ? m->rom[idx] : m->ram[idx];
}

void umk_poke(umk_machine_t *m, uint16_t addr, uint8_t val)
{
    bool is_rom; uint16_t idx;
    if (!decode(m, addr, &is_rom, &idx)) return;
    if (!is_rom) m->ram[idx] = val;
}

/* --- indicadores: integración con persistencia ----------------------------
 *
 * Integrador con fuga por par (dígito, segmento). En cada intervalo dt se
 * suma dt a los pares encendidos y a un acumulador total, y luego todo
 * decae exponencialmente con constante `tau_cycles`. La intensidad de un
 * segmento es su energía dividida por el total, o sea su ciclo de trabajo
 * en la ventana de persistencia.
 *
 * El decaimiento se aplica a pasos fijos de tau/64 mediante desplazamientos,
 * para no meter una división de 64 bits por celda y por instrucción.
 */
#define ENERGY_SCALE 256u
#define DECAY_SHIFT  6u
#define DECAY_MAX_STEPS 512u

static void display_decay_step(umk_display_t *d)
{
    unsigned i, j;
    for (i = 0; i < UMK_DIGITS; i++) {
        for (j = 0; j < UMK_SEGMENTS; j++)
            d->energy[i][j] -= d->energy[i][j] >> DECAY_SHIFT;
        d->energy_digit[i] -= d->energy_digit[i] >> DECAY_SHIFT;
    }
    d->total -= d->total >> DECAY_SHIFT;
}

static void display_advance(umk_machine_t *m, uint64_t to_cycle)
{
    umk_display_t *d = &m->display;
    uint64_t delta;
    uint32_t dt, step_size, steps;
    uint8_t sel, seg;
    unsigned bit, s, pos;

    if (to_cycle <= d->integrated_to) return;

    delta = to_cycle - d->integrated_to;
    d->integrated_to = to_cycle;
    dt = (delta > 0x00100000ull) ? 0x00100000u : (uint32_t)delta;

    /* Qué está encendido durante este intervalo. Sólo cuentan los seis bits
     * bajos de PORTA: el УМК-80 tiene seis indicadores. */
    sel = (uint8_t)(m->ppi.out_a & 0x3Fu);
    seg = m->ppi.out_b;

    for (bit = 0; bit < UMK_DIGITS; bit++) {
        if ((sel & (1u << bit)) == 0u) continue;
        pos = d->bit0_is_left ? bit : (UMK_DIGITS - 1u - bit);
        d->energy_digit[pos] += dt * ENERGY_SCALE;
        for (s = 0; s < UMK_SEGMENTS; s++) {
            if (seg & (1u << s)) d->energy[pos][s] += dt * ENERGY_SCALE;
        }
    }
    d->total += dt * ENERGY_SCALE;

    step_size = d->tau_cycles >> DECAY_SHIFT;
    if (step_size == 0u) step_size = 1u;

    d->decay_acc += dt;
    steps = 0u;
    while (d->decay_acc >= step_size && steps < DECAY_MAX_STEPS) {
        d->decay_acc -= step_size;
        display_decay_step(d);
        steps++;
    }
    if (steps >= DECAY_MAX_STEPS) {
        /* Han pasado eras sin refrescar: todo apagado. */
        zero(d->energy, sizeof d->energy);
        zero(d->energy_digit, sizeof d->energy_digit);
        d->total = 0u;
        d->decay_acc = 0u;
    }
}

void umk_display_intensity(const umk_machine_t *m,
                           uint8_t out[UMK_DIGITS][UMK_SEGMENTS])
{
    const umk_display_t *d = &m->display;
    unsigned i, j;
    for (i = 0; i < UMK_DIGITS; i++) {
        for (j = 0; j < UMK_SEGMENTS; j++) {
            uint32_t v = 0u;
            if (d->total != 0u) {
                uint64_t t = ((uint64_t)d->energy[i][j] * 255u) / d->total;
                v = (t > 255u) ? 255u : (uint32_t)t;
            }
            out[i][j] = (uint8_t)v;
        }
    }
}

void umk_display_relative(const umk_machine_t *m,
                          uint8_t out[UMK_DIGITS][UMK_SEGMENTS])
{
    const umk_display_t *d = &m->display;
    unsigned i, j;
    for (i = 0; i < UMK_DIGITS; i++) {
        for (j = 0; j < UMK_SEGMENTS; j++) {
            uint32_t v = 0u;
            if (d->energy_digit[i] != 0u) {
                uint64_t t = ((uint64_t)d->energy[i][j] * 255u) / d->energy_digit[i];
                v = (t > 255u) ? 255u : (uint32_t)t;
            }
            out[i][j] = (uint8_t)v;
        }
    }
}

void umk_display_pattern(const umk_machine_t *m, uint8_t threshold,
                         uint8_t out[UMK_DIGITS])
{
    uint8_t inten[UMK_DIGITS][UMK_SEGMENTS];
    unsigned i, j;
    umk_display_relative(m, inten);
    for (i = 0; i < UMK_DIGITS; i++) {
        uint8_t mask = 0u;
        for (j = 0; j < UMK_SEGMENTS; j++)
            if (inten[i][j] > threshold) mask = (uint8_t)(mask | (1u << j));
        out[i] = mask;
    }
}

void umk_display_clear_accumulator(umk_machine_t *m)
{
    zero(m->display.energy, sizeof m->display.energy);
    zero(m->display.energy_digit, sizeof m->display.energy_digit);
    m->display.total = 0u;
    m->display.decay_acc = 0u;
    m->display.integrated_to = m->cpu.cycles;
}

void umk_display_set_digit_order(umk_machine_t *m, umk_digit_order_t order)
{
    m->display.bit0_is_left = (order == UMK_DIGIT_BIT0_LEFT);
    umk_display_clear_accumulator(m);
}

void umk_display_set_persistence(umk_machine_t *m, uint32_t tau_cycles)
{
    m->display.tau_cycles = (tau_cycles == 0u) ? 1u : tau_cycles;
    m->display.decay_acc = 0u;
}

/* --- teclado -------------------------------------------------------------- */

/* Bits de PORTC en los que aparece cada fila (máscara 74h). */
static const uint8_t ROW_BIT[UMK_KEY_ROWS] = { 2u, 4u, 5u, 6u };

static uint8_t keyboard_scan(const umk_machine_t *m)
{
    uint8_t value = 0xFFu;           /* reposo: todas las filas a nivel alto */
    uint8_t cols = (uint8_t)(m->ppi.out_a & 0x3Fu);
    unsigned c, r;

    for (c = 0; c < UMK_KEY_COLS; c++) {
        if ((cols & (1u << c)) == 0u) continue;
        for (r = 0; r < UMK_KEY_ROWS; r++) {
            if (m->kbd.pressed[c][r])
                value = (uint8_t)(value & ~(uint8_t)(1u << ROW_BIT[r]));
        }
    }
    return value;
}

void umk_set_key(umk_machine_t *m, unsigned col, unsigned row, bool down)
{
    if (col < UMK_KEY_COLS && row < UMK_KEY_ROWS) m->kbd.pressed[col][row] = down;
}

void umk_release_all_keys(umk_machine_t *m)
{
    zero(&m->kbd, sizeof m->kbd);
}

/* --- КР580ВВ55А ----------------------------------------------------------- */

static void ppi_write_control(umk_ppi_t *p, uint8_t val)
{
    if (val & 0x80u) {
        /* Palabra de modo. El 8255 borra los pestillos de salida. */
        p->ctrl = val;
        p->a_is_out     = (val & 0x10u) == 0u;
        p->c_hi_is_out  = (val & 0x08u) == 0u;
        p->b_is_out     = (val & 0x02u) == 0u;
        p->c_lo_is_out  = (val & 0x01u) == 0u;
        p->out_a = 0u; p->out_b = 0u; p->out_c = 0u;
    } else {
        /* Bit set/reset sobre el puerto C. */
        unsigned bit = (unsigned)((val >> 1) & 7u);
        if (val & 1u) p->out_c = (uint8_t)(p->out_c | (1u << bit));
        else          p->out_c = (uint8_t)(p->out_c & ~(uint8_t)(1u << bit));
    }
}

/* --- bus ------------------------------------------------------------------ */

static uint8_t bus_read(void *ud, uint16_t addr, uint8_t status)
{
    (void)status;
    return umk_peek((const umk_machine_t *)ud, addr);
}

static void bus_write(void *ud, uint16_t addr, uint8_t val, uint8_t status)
{
    (void)status;
    umk_poke((umk_machine_t *)ud, addr, val);
}

static uint8_t bus_in(void *ud, uint8_t port)
{
    umk_machine_t *m = (umk_machine_t *)ud;
    switch (port) {
        case UMK_PORT_A:    return m->ppi.a_is_out ? m->ppi.out_a : 0xFFu;
        case UMK_PORT_B:    return m->ppi.b_is_out ? m->ppi.out_b : 0xFFu;
        case UMK_PORT_C:    return keyboard_scan(m);
        case UMK_PORT_CTRL: return 0xFFu;   /* el 8255 no deja leer el control */
        case UMK_PORT_DBG:  return m->step.dbg_port;
        default:            return 0xFFu;
    }
}

/* Dentro de una instrucción OUT (10 ciclos T: M1 = 4, lectura del número de
 * puerto = 3, escritura = 3) la escritura ocurre en el último ciclo de
 * máquina, o sea a partir del ciclo T número 7. Situar ahí el cambio de
 * puerto importa: el fantasmeo del multiplexado se juega en intervalos de
 * una decena de ciclos T entre un OUT 0F8H y el siguiente OUT 0F9H. */
#define OUT_WRITE_OFFSET 7u

static void bus_out(void *ud, uint8_t port, uint8_t val)
{
    umk_machine_t *m = (umk_machine_t *)ud;

    switch (port) {
        case UMK_PORT_A:
            display_advance(m, m->step_start_cycles + OUT_WRITE_OFFSET);
            if (m->ppi.a_is_out) m->ppi.out_a = val;
            break;
        case UMK_PORT_B:
            display_advance(m, m->step_start_cycles + OUT_WRITE_OFFSET);
            if (m->ppi.b_is_out) m->ppi.out_b = val;
            break;
        case UMK_PORT_C:
            if (m->ppi.c_lo_is_out) m->ppi.out_c = (uint8_t)((m->ppi.out_c & 0xF0u) | (val & 0x0Fu));
            if (m->ppi.c_hi_is_out) m->ppi.out_c = (uint8_t)((m->ppi.out_c & 0x0Fu) | (val & 0xF0u));
            break;
        case UMK_PORT_CTRL:
            display_advance(m, m->step_start_cycles + OUT_WRITE_OFFSET);
            ppi_write_control(&m->ppi, val);
            break;
        case UMK_PORT_DBG:
            m->step.dbg_port = val;
            break;
        default:
            m->unmapped_io_writes++;
            break;
    }
}

static const i8080_bus_t BUS = {
    bus_read, bus_write, bus_in, bus_out, NULL
};

static i8080_bus_t bus_for(umk_machine_t *m)
{
    i8080_bus_t b = BUS;
    b.ud = m;
    return b;
}

/* --- panel ---------------------------------------------------------------- */

static void panel_show_mc(umk_machine_t *m, unsigned i)
{
    if (i < m->cpu.mc_count) {
        m->panel.address = m->cpu.mc_addr[i];
        m->panel.data    = m->cpu.mc_data[i];
        m->panel.status  = m->cpu.mc_status[i];
    }
}

static void panel_update(umk_machine_t *m)
{
    if (m->cpu.mc_count > 0u) panel_show_mc(m, (unsigned)(m->cpu.mc_count - 1u));
}

void umk_set_power_fault(umk_machine_t *m, bool p5, bool m5, bool p12)
{
    m->panel.fault_p5  = p5;
    m->panel.fault_m5  = m5;
    m->panel.fault_p12 = p12;
}

/* --- ciclo de vida -------------------------------------------------------- */

void umk_init(umk_machine_t *m, umk_rev_t rev)
{
    zero(m, sizeof *m);

    m->magic    = UMK_MAGIC;
    m->version  = UMK_VERSION;
    m->rev      = rev;
    m->rom_size = UMK_ROM_MAX;
    m->ram_size = (rev == UMK_REV1) ? 1024u : UMK_RAM_MAX;
    m->clock_hz = 2000000u;

    i8080_reset(&m->cpu);

    /* Tras el encendido el ВВ55 arranca con los tres puertos como entrada,
     * que es el estado por omisión del 8255 real. El monitor lo reprograma
     * con la palabra 89h (МОН: MVI A, NOT CNTRWRD). */
    ppi_write_control(&m->ppi, 0x9Bu);

    m->display.bit0_is_left = true;             /* DESCONOCIDOS.md §3 */
    m->display.tau_cycles   = m->clock_hz / 50u;/* ~20 ms de persistencia */
    m->display.integrated_to = 0u;
}

bool umk_load_rom(umk_machine_t *m, uint16_t offset,
                  const uint8_t *data, size_t len)
{
    if ((size_t)offset + len > m->rom_size) return false;
    copy(m->rom + offset, data, len);
    m->rom_present = true;
    return true;
}

void umk_load_ram(umk_machine_t *m, uint16_t addr,
                  const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) umk_poke(m, (uint16_t)(addr + i), data[i]);
}

void umk_reset(umk_machine_t *m)
{
    i8080_reset(&m->cpu);
    ppi_write_control(&m->ppi, 0x9Bu);
    m->step.waiting = m->step.latch_step;
    m->step.mc_index = 0u;
    m->step_start_cycles = 0u;
    umk_display_clear_accumulator(m);
    zero(&m->panel.address, sizeof m->panel.address);
    m->panel.data = 0u;
    m->panel.status = 0u;
}

void umk_interrupt(umk_machine_t *m)
{
    /* ПР genera RST 7: el monitor lo atiende en 0038h. МОН hoja −19−,
     * «ОБРАБАТЫВАЮЩИЕ ПРОГРАММЫ / RESTART». */
    i8080_interrupt(&m->cpu, 0xFFu);
}

/* --- ejecución ------------------------------------------------------------ */

static unsigned exec_one(umk_machine_t *m)
{
    i8080_bus_t bus = bus_for(m);
    unsigned c;

    m->step_start_cycles = m->cpu.cycles;
    c = i8080_step(&m->cpu, &bus);
    display_advance(m, m->cpu.cycles);
    return c;
}

unsigned umk_step_instruction(umk_machine_t *m)
{
    unsigned c = exec_one(m);
    m->step.mc_index = 0u;
    panel_update(m);
    return c;
}

bool umk_step_machine_cycle(umk_machine_t *m)
{
    bool last;

    if (m->step.mc_index == 0u) exec_one(m);

    panel_show_mc(m, m->step.mc_index);
    m->step.mc_index++;

    last = (m->step.mc_index >= m->cpu.mc_count);
    if (last) m->step.mc_index = 0u;
    return last;
}

uint64_t umk_run_cycles(umk_machine_t *m, uint64_t cycles)
{
    uint64_t start = m->cpu.cycles;

    if (m->step.latch_step) return 0u;   /* detenida esperando ШГ */

    while (m->cpu.cycles - start < cycles) {
        exec_one(m);
    }
    panel_update(m);
    return m->cpu.cycles - start;
}

/* --- mandos --------------------------------------------------------------- */

void umk_set_switch(umk_machine_t *m, umk_switch_t sw, bool latched)
{
    if (sw == UMK_SW_STEP) {
        m->step.latch_step = latched;
        m->step.waiting = latched;
        if (!latched) m->step.mc_index = 0u;
    } else {
        m->step.latch_cycle = latched;
        m->step.mc_index = 0u;
    }
}

bool umk_get_switch(const umk_machine_t *m, umk_switch_t sw)
{
    return (sw == UMK_SW_STEP) ? m->step.latch_step : m->step.latch_cycle;
}

void umk_press_step(umk_machine_t *m)
{
    if (!m->step.latch_step) return;
    if (m->step.latch_cycle) (void)umk_step_machine_cycle(m);
    else                     (void)umk_step_instruction(m);
}

/* --- estado --------------------------------------------------------------- */

size_t umk_state_size(void) { return sizeof(umk_machine_t); }

bool umk_state_save(const umk_machine_t *m, void *buf, size_t len)
{
    if (len < sizeof *m) return false;
    copy(buf, m, sizeof *m);
    return true;
}

bool umk_state_load(umk_machine_t *m, const void *buf, size_t len)
{
    const umk_machine_t *src = (const umk_machine_t *)buf;
    if (len < sizeof *m) return false;
    if (src->magic != UMK_MAGIC || src->version != UMK_VERSION) return false;
    copy(m, buf, sizeof *m);
    return true;
}
