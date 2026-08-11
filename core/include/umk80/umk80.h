/* umk80.h — núcleo de emulación del УМК-80 (ВЭФ, РР3.059.004).
 *
 * API pública del núcleo. Independiente: sólo <stdint.h>, <stddef.h> y
 * <stdbool.h>. Sin asignación dinámica, sin estado global, sin E/S de
 * consola ni de interfaz gráfica. Toda la máquina vive en un `umk_machine_t`
 * que el llamante posee y que no contiene punteros, de modo que guardar y
 * restaurar el estado completo es copiar la estructura.
 *
 * Referencias documentales (docs/FUENTES.md):
 *   ПС  = РР3.059.004 ПС, паспорт
 *   МОН = Р.Р.00004-01 12 01-1, Системный монитор, текст программы
 */
#ifndef UMK80_UMK80_H
#define UMK80_UMK80_H

#include "umk80/i8080.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Perfiles de máquina -------------------------------------------------
 *
 * La documentación descargada corresponde a la revisión 2 (ПС p. 8 con la
 * corrección manuscrita del doc. РР1323-87, 10.09.89): un solo chip de ОЗУ
 * КР537РУ8А de 2 K×8 y un solo chip de ПЗУ К573РФ2 de 2 K×8. El propio
 * monitor lo confirma con RAMEND = 1000H.
 *
 * La revisión 1 es la que describen eax.me y el encargo original: 2 KB de
 * ПЗУ en dos К573РФ1 y 1 KB de ОЗУ. Para que el monitor funcione en esa
 * revisión el ОЗУ tiene que estar espejado en 0800h-0FFFh, porque el
 * monitor usa 0FFAh. Ver DESCONOCIDOS.md §1.
 */
typedef enum {
    UMK_REV1 = 1,   /* ПЗУ 2 KB + ОЗУ 1 KB espejado */
    UMK_REV2 = 2    /* ПЗУ 2 KB + ОЗУ 2 KB (documentada) */
} umk_rev_t;

#define UMK_ROM_MAX     2048u
#define UMK_RAM_MAX     2048u
#define UMK_ROM_BASE    0x0000u
#define UMK_RAM_BASE    0x0800u
#define UMK_DIGITS      6u
#define UMK_SEGMENTS    8u          /* A..G más el punto decimal */
#define UMK_KEY_ROWS    4u
#define UMK_KEY_COLS    6u

/* Puertos, tal y como los declara el propio monitor (МОН hoja −4−). */
#define UMK_PORT_A      0xF8u       /* ПОРТ АДРЕСА:   selección de indicador */
#define UMK_PORT_B      0xF9u       /* ПОРТ ДАННЫХ:   máscara de segmentos   */
#define UMK_PORT_C      0xFAu       /* ПОРТ СОСТОЯНИЯ: filas del teclado     */
#define UMK_PORT_CTRL   0xFBu       /* ПОРТ УПРАВЛ. БИС: control del ВВ55    */
#define UMK_PORT_DBG    0xFCu       /* ПОРТ ПОШАГ. РЕЖ.                      */

/* Bits de PORTC en los que están las cuatro filas del teclado: la máscara
 * 74h que usan CILOOP (МОН hoja −30−) y la rutina de espera de suelta. */
#define UMK_KEY_ROW_MASK 0x74u

/* --- КР580ВВ55А (8255) --------------------------------------------------- */
typedef struct {
    uint8_t ctrl;        /* última palabra de modo escrita (bit 7 = 1) */
    uint8_t out_a;       /* pestillo del puerto A */
    uint8_t out_b;       /* pestillo del puerto B */
    uint8_t out_c;       /* pestillo del puerto C (parte que sea salida) */
    bool    a_is_out;
    bool    b_is_out;
    bool    c_lo_is_out;
    bool    c_hi_is_out;
} umk_ppi_t;

/* --- Indicadores de siete segmentos --------------------------------------
 *
 * No se modelan como seis dígitos con estado propio: se integra la energía
 * de cada par (dígito, segmento) a lo largo del tiempo simulado, con
 * decaimiento exponencial. Un programa que cambie la máscara de segmentos
 * sin apagar antes los indicadores produce por construcción el fantasmeo
 * característico, sin ningún código que lo simule.
 */
typedef struct {
    uint32_t energy[UMK_DIGITS][UMK_SEGMENTS];  /* punto fijo, escala 1/256 */
    uint32_t energy_digit[UMK_DIGITS];          /* tiempo con ese dígito seleccionado */
    uint32_t total;                             /* misma escala; normaliza  */
    uint64_t integrated_to;                     /* ciclo T hasta el que se integró */
    uint32_t tau_cycles;                        /* constante de persistencia */
    uint32_t decay_acc;                         /* resto entre pasos de decaimiento */
    bool     bit0_is_left;                      /* orientación de los dígitos */
} umk_display_t;

/* --- Panel de LEDs -------------------------------------------------------
 *
 * АДРЕС (16), ДАННЫЕ (8) y СОСТОЯНИЕ (8) son pestillos del hardware que
 * siguen el último ciclo de máquina ejecutado.
 *
 * Los tres LEDs de alimentación son indicadores de AVERÍA: encendido
 * significa que esa tensión FALTA. Ver DESCONOCIDOS.md §8.
 */
typedef struct {
    uint16_t address;
    uint8_t  data;
    uint8_t  status;     /* palabra I8080_ST_* del último ciclo de máquina */
    bool     fault_p5;   /* true = falta +5 V  -> LED encendido */
    bool     fault_m5;   /* true = falta -5 V  */
    bool     fault_p12;  /* true = falta +12 V */
} umk_panel_t;

/* --- Modo paso a paso ----------------------------------------------------
 *
 * РБ/ШГ (S3) y КМ/ЦК (S4) son conmutadores CON ENCLAVAMIENTO: quedan
 * pulsados (ПС p. 28). ШГ (S2), СБ (S1) y ПР (S5) son pulsadores.
 *
 * Cómo se modela el paso por ciclo de máquina: en la primera pulsación se
 * ejecuta la instrucción «en seco» para averiguar su secuencia de ciclos y
 * acto seguido se deshace por completo (registros, ОЗУ, ВВ55 e indicadores).
 * Las pulsaciones siguientes van mostrando en el panel cada ciclo de la
 * secuencia, y sólo al pisar el ÚLTIMO se ejecuta de verdad. Así el estado
 * de la máquina cambia exactamente cuando cambia en el equipo real, donde la
 * CPU está congelada por READY entre ciclo y ciclo.
 *
 * Lo que sigue sin ser fiel al detalle: dentro de una instrucción con varias
 * escrituras (CALL, PUSH) todas caen juntas en el último ciclo en vez de
 * cada una en el suyo. No es observable desde el panel, que es lo único que
 * la máquina enseña en este modo. Ver DESCONOCIDOS.md §7.
 */
typedef struct {
    bool    latch_step;      /* РБ/ШГ enclavado: modo paso a paso activo */
    bool    latch_cycle;     /* КМ/ЦК enclavado: el paso es por ciclo    */
    bool    waiting;         /* la CPU está detenida esperando ШГ        */
    uint8_t dbg_port;        /* último valor escrito en 0FCh             */

    /* Secuencia de ciclos de máquina de la instrucción en curso, obtenida
     * por ejecución en seco. */
    uint8_t  mc_index;
    uint8_t  mc_total;
    uint8_t  mc_status[6];
    uint16_t mc_addr[6];
    uint8_t  mc_data[6];
} umk_step_t;

/* --- Teclado -------------------------------------------------------------
 *
 * Matriz de 6 columnas × 4 filas = 24 teclas (ПС p. 34: «24 клавиш, из них
 * 8 клавиш директивные, а 16 — информационные»). Las columnas son las
 * mismas seis líneas de PORTA que seleccionan el dígito; las filas son los
 * bits 2, 4, 5 y 6 de PORTC, con reposo a nivel alto.
 *
 * Qué tecla física ocupa cada intersección está pendiente de transcribir
 * (DESCONOCIDOS.md §4); por eso la matriz es un dato configurable y no una
 * tabla cableada.
 */
typedef struct {
    bool pressed[UMK_KEY_COLS][UMK_KEY_ROWS];
} umk_keyboard_t;

/* --- La máquina ----------------------------------------------------------- */
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

    /* Instante en el que empezó la instrucción en curso; lo usa la
     * integración del display para situar el OUT dentro de la instrucción. */
    uint64_t      step_start_cycles;

    /* Contador de escrituras a puertos no decodificados: útil para detectar
     * programas que hablan con hardware que este equipo no tiene. */
    uint32_t      unmapped_io_writes;

    /* Diario de escrituras en ОЗУ durante la ejecución en seco del modo
     * paso por ciclo de máquina. Nunca hace falta más de una entrada por
     * ciclo de máquina. */
    bool          trial;
    uint8_t       trial_n;
    uint16_t      trial_idx[8];
    uint8_t       trial_old[8];
} umk_machine_t;

/* --- Ciclo de vida -------------------------------------------------------- */

/* Deja la máquina en el estado de encendido: ОЗУ a cero, ПЗУ sin cargar,
 * CPU en reset, ВВ55 sin programar, sin averías de alimentación. */
void umk_init(umk_machine_t *m, umk_rev_t rev);

/* Carga una imagen en el ПЗУ a partir de `offset` (0 para el monitor,
 * 0x400 para el ПРОГРАММАТОР). Devuelve false si no cabe. */
bool umk_load_rom(umk_machine_t *m, uint16_t offset,
                  const uint8_t *data, size_t len);

/* Carga bytes en la memoria por el mismo camino que vería la CPU
 * (respeta la decodificación: escribir en ПЗУ no tiene efecto). */
void umk_load_ram(umk_machine_t *m, uint16_t addr,
                  const uint8_t *data, size_t len);

/* Pulsador СБ (S1). */
void umk_reset(umk_machine_t *m);

/* Pulsador ПР (S5): petición de interrupción. En el УМК-80 el vector es
 * RST 7 (opcode 0FFh), que el monitor atiende en 0038h para guardar el
 * estado de todos los registros en ОЗУ. */
void umk_interrupt(umk_machine_t *m);

/* --- Ejecución ------------------------------------------------------------ */

/* Ejecuta instrucciones hasta consumir al menos `cycles` ciclos T.
 * Devuelve los ciclos realmente consumidos (puede pasarse por el último
 * salto de instrucción). No hace nada si la máquina está detenida
 * esperando ШГ. */
uint64_t umk_run_cycles(umk_machine_t *m, uint64_t cycles);

/* Ejecuta exactamente una instrucción. Devuelve sus ciclos T. */
unsigned umk_step_instruction(umk_machine_t *m);

/* Avanza un ciclo de máquina (pulsación de ШГ con КМ/ЦК enclavado).
 * Devuelve true si con este ciclo se completó la instrucción. */
bool umk_step_machine_cycle(umk_machine_t *m);

/* --- Mandos --------------------------------------------------------------- */

typedef enum {
    UMK_SW_STEP  = 0,   /* РБ/ШГ (S3) */
    UMK_SW_CYCLE = 1    /* КМ/ЦК (S4), documentado también como ММ/ЦИ */
} umk_switch_t;

void umk_set_switch(umk_machine_t *m, umk_switch_t sw, bool latched);
bool umk_get_switch(const umk_machine_t *m, umk_switch_t sw);

/* Pulsador ШГ (S2). Sin efecto si РБ/ШГ no está enclavado. */
void umk_press_step(umk_machine_t *m);

/* Tecla de la matriz. `col` en 0..5, `row` en 0..3. */
void umk_set_key(umk_machine_t *m, unsigned col, unsigned row, bool down);
void umk_release_all_keys(umk_machine_t *m);

/* Averías de alimentación (para poder ejercitar los LEDs). */
void umk_set_power_fault(umk_machine_t *m, bool p5, bool m5, bool p12);

/* --- Display -------------------------------------------------------------- */

/* Orientación de los indicadores. Resuelto en DESCONOCIDOS.md §3: el bit 0
 * de PORTA es el indicador de más a la izquierda. */
typedef enum {
    UMK_DIGIT_BIT0_LEFT  = 0,   /* por omisión, y lo que dice la documentación */
    UMK_DIGIT_BIT0_RIGHT = 1
} umk_digit_order_t;

void umk_display_set_digit_order(umk_machine_t *m, umk_digit_order_t order);

/* Constante de persistencia del indicador, en ciclos T. Por omisión
 * equivale a unos 20 ms al reloj configurado. Ver DESCONOCIDOS.md §5. */
void umk_display_set_persistence(umk_machine_t *m, uint32_t tau_cycles);

/* Brillo ABSOLUTO de cada segmento, 0..255, indexado por posición en el
 * panel (0 = el de más a la izquierda). 255 sería el segmento encendido de
 * forma continua; en un display multiplexado a seis dígitos el máximo real
 * ronda 255/6. Es lo que debe dibujar el frontend. */
void umk_display_intensity(const umk_machine_t *m,
                           uint8_t out[UMK_DIGITS][UMK_SEGMENTS]);

/* Brillo RELATIVO A SU PROPIO DÍGITO, 0..255: 255 significa «ese segmento
 * estuvo encendido siempre que su dígito estuvo seleccionado». Es la
 * magnitud que dice qué carácter muestra cada indicador, y por tanto la que
 * revela el fantasmeo: un dígito limpio da 255 o 0 en cada segmento, y uno
 * fantasmeado da valores intermedios en los segmentos de la letra intrusa. */
void umk_display_relative(const umk_machine_t *m,
                          uint8_t out[UMK_DIGITS][UMK_SEGMENTS]);

/* Máscara de segmentos «promediada» de cada dígito: bit s a 1 si el brillo
 * relativo de ese segmento supera `threshold` (0..255). Con un umbral bajo
 * (p. ej. 25, o sea el 10 %) la máscara recoge también la letra intrusa,
 * que es como se detecta el fantasmeo en la prueba automatizada. */
void umk_display_pattern(const umk_machine_t *m, uint8_t threshold,
                         uint8_t out[UMK_DIGITS]);

/* Reinicia la ventana de integración (para promediar un intervalo concreto
 * en las pruebas automatizadas). */
void umk_display_clear_accumulator(umk_machine_t *m);

/* --- Estado --------------------------------------------------------------- */

/* Tamaño del volcado de estado. */
size_t umk_state_size(void);

/* Guarda / restaura el estado completo de la máquina. `buf` debe tener al
 * menos umk_state_size() bytes. Restaurar comprueba el número mágico y la
 * versión, y devuelve false si no encajan. */
bool umk_state_save(const umk_machine_t *m, void *buf, size_t len);
bool umk_state_load(umk_machine_t *m, const void *buf, size_t len);

/* --- Acceso directo (depurador, pruebas) ---------------------------------- */

/* Lectura y escritura por el camino de la CPU, incluida la decodificación
 * incompleta. `umk_peek` no altera el panel ni el display. */
uint8_t umk_peek(const umk_machine_t *m, uint16_t addr);
void    umk_poke(umk_machine_t *m, uint16_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif
#endif /* UMK80_UMK80_H */
