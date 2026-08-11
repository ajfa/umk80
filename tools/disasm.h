/* disasm.h — desensamblador 8080, compartido por umkdis y por el depurador
 * de umkcli. Vive en tools/ y no en core/ porque usa snprintf, y el núcleo
 * no puede depender de la biblioteca estándar.
 */
#ifndef UMK80_DISASM_H
#define UMK80_DISASM_H

#include <stddef.h>

/* Escribe en `out` el texto de la instrucción que empieza en mem[pc] y
 * devuelve su longitud en bytes (1, 2 o 3). `size` es el tamaño de `mem`. */
unsigned umk_disasm(const unsigned char *mem, unsigned long pc,
                    unsigned long size, char *out, size_t outn);

#endif
