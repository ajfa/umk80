/* i8080.h — núcleo de CPU Intel 8080 / КР580ВМ80А.
 *
 * Independiente (freestanding): sólo <stdint.h>, <stddef.h> y <stdbool.h>.
 * Sin asignación dinámica, sin estado global, sin E/S.
 *
 * `i8080_t` es un POD puro: no contiene punteros. Guardar y restaurar el
 * estado de la CPU es una copia de la estructura. Los accesos al exterior
 * van por `i8080_bus_t`, que el llamante pasa en cada paso y que NO forma
 * parte del estado.
 */
#ifndef UMK80_I8080_H
#define UMK80_I8080_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Palabra de estado del 8080 -----------------------------------------
 *
 * El КР580ВМ80А emite en D0..D7, durante SYNC, una palabra de estado que en
 * el УМК-80 queda enganchada en el registro D4 y se muestra en la fila de
 * LEDs «СОСТОЯНИЕ». Los rótulos del panel, de bit 7 a bit 0, son:
 *
 *   MEMR  INP  M1  OUT  HLTA  STACK  WO  INTA
 *
 * Ojo con WO (bit 1): es activo a nivel bajo. WO = 1 significa lectura;
 * WO = 0 significa escritura o salida.
 */
#define I8080_ST_INTA   0x01u
#define I8080_ST_WO     0x02u
#define I8080_ST_STACK  0x04u
#define I8080_ST_HLTA   0x08u
#define I8080_ST_OUT    0x10u
#define I8080_ST_M1     0x20u
#define I8080_ST_INP    0x40u
#define I8080_ST_MEMR   0x80u

/* Las diez palabras de estado que el 8080 puede emitir. */
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

/* --- Banderas del registro de condiciones (PSW) --------------------------
 *
 * En el 8080 el bit 1 vale siempre 1 y los bits 3 y 5 valen siempre 0.
 * Esto se preserva en PUSH PSW, en POP PSW y en el propio registro.
 */
#define I8080_F_C   0x01u
#define I8080_F_ONE 0x02u   /* siempre 1 */
#define I8080_F_P   0x04u
#define I8080_F_AC  0x10u
#define I8080_F_Z   0x40u
#define I8080_F_S   0x80u

/* Máscara de los bits que existen realmente. */
#define I8080_F_MASK (I8080_F_C | I8080_F_P | I8080_F_AC | I8080_F_Z | I8080_F_S)

/* --- Estado de la CPU ---------------------------------------------------- */
typedef struct {
    uint8_t  a, f;
    uint8_t  b, c, d, e, h, l;
    uint16_t sp, pc;

    bool     inte;          /* biestable de habilitación de interrupción */
    bool     halted;        /* dentro de HLT */
    bool     int_pending;   /* hay una petición de interrupción sin atender */
    uint8_t  int_vector;    /* opcode que la lógica externa mete en el ciclo INTA */
    uint8_t  int_delay;     /* tras EI aún se ejecuta una instrucción más antes
                             * de poder reconocer una interrupción; DI lo anula */

    uint64_t cycles;        /* ciclos de reloj T consumidos desde el reset */

    /* Traza de ciclos de máquina de la última instrucción ejecutada.
     * Sirve al panel de LEDs y al modo paso por ciclo de máquina (КМ/ЦК).
     * El 8080 no pasa de 5 ciclos de máquina por instrucción. */
    uint8_t  mc_count;
    uint8_t  mc_status[6];
    uint16_t mc_addr[6];
    uint8_t  mc_data[6];
} i8080_t;

/* --- Interfaz con el exterior -------------------------------------------
 *
 * `status` es la palabra I8080_CYC_* del ciclo de máquina en curso, de modo
 * que el sistema puede distinguir una búsqueda de instrucción de una lectura
 * de dato o de pila sin reconstruirlo.
 */
typedef struct {
    uint8_t (*read)(void *ud, uint16_t addr, uint8_t status);
    void    (*write)(void *ud, uint16_t addr, uint8_t val, uint8_t status);
    uint8_t (*in)(void *ud, uint8_t port);
    void    (*out)(void *ud, uint8_t port, uint8_t val);
    void    *ud;
} i8080_bus_t;

/* --- API ----------------------------------------------------------------- */

/* Pone la CPU en el estado que tiene tras RESET: PC = 0, INTE = 0,
 * HLTA = 0. Los registros del 8080 real quedan indeterminados tras el
 * reset; aquí se ponen a cero de forma reproducible. */
void i8080_reset(i8080_t *cpu);

/* Ejecuta una instrucción completa (o atiende una interrupción pendiente).
 * Devuelve los ciclos de reloj T consumidos y los acumula en cpu->cycles.
 * Deja la traza de ciclos de máquina en cpu->mc_*. */
unsigned i8080_step(i8080_t *cpu, const i8080_bus_t *bus);

/* Solicita una interrupción. `vector` es el byte que la lógica externa
 * pondrá en el bus durante el ciclo INTA; en el УМК-80 el botón ПР genera
 * RST 7, o sea 0xFF. La petición se atiende en el siguiente i8080_step()
 * si INTE está activo. */
void i8080_interrupt(i8080_t *cpu, uint8_t vector);

/* Ciclos de reloj T de un opcode, sin contar el camino tomado en saltos
 * y llamadas condicionales (para ésos devuelve el caso NO tomado). Útil
 * para el desensamblador y para presupuestos de tiempo. */
unsigned i8080_opcode_cycles(uint8_t opcode);

/* Longitud en bytes de un opcode: 1, 2 o 3. */
unsigned i8080_opcode_length(uint8_t opcode);

/* true si el opcode no está documentado por Intel (los siete NOP alternativos,
 * el JMP alternativo, el RET alternativo y los tres CALL alternativos). */
bool i8080_opcode_undocumented(uint8_t opcode);

#ifdef __cplusplus
}
#endif
#endif /* UMK80_I8080_H */
