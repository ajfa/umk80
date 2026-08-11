/* criterio4.c — criterio de aceptación 4 del encargo.
 *
 *   «En modo paso a paso (РБ/ШГ enclavado, ММ/ЦИ suelto), cada pulsación de
 *    ШГ avanza exactamente una instrucción; con ММ/ЦИ enclavado, exactamente
 *    un ciclo de máquina.»
 *
 * (La documentación oficial llama КМ/ЦК a ese conmutador; ПС p. 28.)
 *
 * Se comprueba, además de la cuenta de pulsaciones:
 *   - que con РБ/ШГ enclavado la máquina está de verdad congelada;
 *   - que en modo ciclo el estado sólo cambia al pisar el ÚLTIMO ciclo de
 *     máquina de la instrucción, no antes;
 *   - que el panel АДРЕС/ДАННЫЕ/СОСТОЯНИЕ sigue ciclo a ciclo lo que va por
 *     el bus, con la palabra de estado correcta en cada uno.
 */

#include "umk80/umk80.h"

#include <stdio.h>

/* Programa de prueba, elegido para que haya instrucciones de 1, 2 y 3 ciclos
 * de máquina y una escritura en memoria observable. */
static const uint8_t PROG[] = {
    0x3E, 0xAA,             /* 0800  MVI A,0AAH   -> 2 ciclos: M1 + lectura  */
    0x06, 0x55,             /* 0802  MVI B,55H    -> 2                        */
    0x21, 0x00, 0x09,       /* 0804  LXI H,0900H  -> 3: M1 + 2 lecturas       */
    0x77,                   /* 0807  MOV M,A      -> 2: M1 + escritura        */
    0x3C,                   /* 0808  INR A        -> 1: sólo M1               */
    0xC3, 0x00, 0x08        /* 0809  JMP 0800H    -> 3                        */
};

static const uint16_t PC_SEQ[6] = { 0x0802, 0x0804, 0x0807, 0x0808, 0x0809, 0x0800 };
static const unsigned MC_SEQ[6] = { 2, 2, 3, 2, 1, 3 };
static const char *const NAME[6] = {
    "MVI A,0AAH", "MVI B,55H", "LXI H,0900H", "MOV M,A", "INR A", "JMP 0800H"
};

static const char *status_name(uint8_t s)
{
    switch (s) {
        case I8080_CYC_FETCH:  return "M1 (búsqueda)";
        case I8080_CYC_MEMR:   return "lectura de memoria";
        case I8080_CYC_MEMW:   return "escritura de memoria";
        case I8080_CYC_STACKR: return "lectura de pila";
        case I8080_CYC_STACKW: return "escritura de pila";
        case I8080_CYC_INPR:   return "entrada";
        case I8080_CYC_OUTW:   return "salida";
        case I8080_CYC_HALTA:  return "parada";
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

    printf("=== Criterio 4: paso por instrucción y por ciclo de máquina ===\n");

    /* ---------------------------------------------------------------------
     * a) РБ/ШГ enclavado congela la máquina.
     * ------------------------------------------------------------------- */
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, false);
    {
        uint64_t before = m.cpu.cycles;
        umk_run_cycles(&m, 1000000u);
        if (m.cpu.cycles != before || m.cpu.pc != 0x0800u) {
            printf("FALLO: con РБ/ШГ enclavado la máquina sigue corriendo\n");
            ok = 0;
        } else {
            printf("\nOK: con РБ/ШГ enclavado la máquina está detenida\n");
        }
    }

    /* ---------------------------------------------------------------------
     * b) Una pulsación de ШГ = una instrucción.
     * ------------------------------------------------------------------- */
    printf("\n-- РБ/ШГ enclavado, КМ/ЦК suelto: una pulsación = una instrucción --\n");
    for (i = 0; i < 6u; i++) {
        umk_press_step(&m);
        printf("  ШГ %u: %-12s  PC=%04X  (esperado %04X)\n",
               i + 1u, NAME[i], m.cpu.pc, PC_SEQ[i]);
        if (m.cpu.pc != PC_SEQ[i]) {
            printf("     FALLO: el PC no avanzó exactamente una instrucción\n");
            ok = 0;
        }
    }
    if (m.cpu.a != 0xABu || m.cpu.b != 0x55u) {
        printf("  FALLO: registros inesperados A=%02X B=%02X\n", m.cpu.a, m.cpu.b);
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0xAAu) {
        printf("  FALLO: MOV M,A no escribió en 0900H\n");
        ok = 0;
    }

    /* ---------------------------------------------------------------------
     * c) Con КМ/ЦК enclavado, una pulsación = un ciclo de máquina.
     * ------------------------------------------------------------------- */
    printf("\n-- РБ/ШГ y КМ/ЦК enclavados: una pulsación = un ciclo de máquina --\n");
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, true);

    for (i = 0; i < 6u; i++) {
        uint16_t pc_before = m.cpu.pc;
        unsigned presses = 0;
        printf("  %-12s (esperados %u ciclos)\n", NAME[i], MC_SEQ[i]);

        for (c = 0; c < 12u; c++) {
            bool done;
            uint8_t mem_before = umk_peek(&m, 0x0900u);

            done = umk_step_machine_cycle(&m);
            presses++;

            printf("     ciclo %u: АДРЕС=%04X ДАННЫЕ=%02X СОСТОЯНИЕ=%02X  %-22s%s\n",
                   presses, m.panel.address, m.panel.data, m.panel.status,
                   status_name(m.panel.status), done ? "  <- último" : "");

            /* Antes del último ciclo no puede haber cambiado nada. */
            if (!done) {
                if (m.cpu.pc != pc_before) {
                    printf("     FALLO: el PC cambió antes del último ciclo\n");
                    ok = 0;
                }
                if (umk_peek(&m, 0x0900u) != mem_before) {
                    printf("     FALLO: la memoria cambió antes del último ciclo\n");
                    ok = 0;
                }
            }
            if (done) break;
        }

        if (presses != MC_SEQ[i]) {
            printf("     FALLO: hicieron falta %u pulsaciones, no %u\n",
                   presses, MC_SEQ[i]);
            ok = 0;
        }
        if (m.cpu.pc != PC_SEQ[i]) {
            printf("     FALLO: PC=%04X tras completar, esperado %04X\n",
                   m.cpu.pc, PC_SEQ[i]);
            ok = 0;
        }
    }

    /* El primer ciclo de cada instrucción tiene que ser una búsqueda (M1) en
     * la dirección de la propia instrucción; y en MOV M,A el segundo ciclo
     * tiene que ser una escritura en 0900H con el dato AAh. Se rehace la
     * comprobación de forma dirigida. */
    printf("\n-- palabra de estado y bus, ciclo a ciclo, en MOV M,A --\n");
    setup(&m);
    umk_set_switch(&m, UMK_SW_STEP, true);
    umk_set_switch(&m, UMK_SW_CYCLE, true);
    for (i = 0; i < 3u; i++)                    /* saltar las tres primeras */
        while (!umk_step_machine_cycle(&m)) { }

    umk_step_machine_cycle(&m);                 /* ciclo 1 de MOV M,A */
    printf("  ciclo 1: АДРЕС=%04X СОСТОЯНИЕ=%02X %s\n",
           m.panel.address, m.panel.status, status_name(m.panel.status));
    if (m.panel.status != I8080_CYC_FETCH || m.panel.address != 0x0807u) {
        printf("     FALLO: se esperaba una búsqueda M1 en 0807H\n");
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0x00u) {
        printf("     FALLO: la escritura se adelantó al ciclo de búsqueda\n");
        ok = 0;
    }

    umk_step_machine_cycle(&m);                 /* ciclo 2: la escritura */
    printf("  ciclo 2: АДРЕС=%04X ДАННЫЕ=%02X СОСТОЯНИЕ=%02X %s\n",
           m.panel.address, m.panel.data, m.panel.status, status_name(m.panel.status));
    if (m.panel.status != I8080_CYC_MEMW || m.panel.address != 0x0900u ||
        m.panel.data != 0xAAu) {
        printf("     FALLO: se esperaba una escritura de AAh en 0900H\n");
        ok = 0;
    }
    if (umk_peek(&m, 0x0900u) != 0xAAu) {
        printf("     FALLO: la escritura no se materializó en su ciclo\n");
        ok = 0;
    }

    /* ---------------------------------------------------------------------
     * d) Soltar РБ/ШГ devuelve la máquina a marcha normal.
     * ------------------------------------------------------------------- */
    umk_set_switch(&m, UMK_SW_STEP, false);
    {
        uint64_t before = m.cpu.cycles;
        umk_run_cycles(&m, 10000u);
        if (m.cpu.cycles <= before) {
            printf("\nFALLO: al soltar РБ/ШГ la máquina no reanuda\n");
            ok = 0;
        } else {
            printf("\nOK: al soltar РБ/ШГ la máquina reanuda la marcha\n");
        }
    }

    printf("\n===== %s =====\n", ok ? "CRITERIO 4 CUMPLIDO" : "CRITERIO 4 NO CUMPLIDO");
    return ok ? 0 : 1;
}
