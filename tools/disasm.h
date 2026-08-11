/* disasm.h — 8080 disassembler, shared by umkdis and by the umkcli debugger.
 * It lives in tools/ rather than core/ because it uses snprintf, and the core
 * cannot depend on the standard library.
 */
#ifndef UMK80_DISASM_H
#define UMK80_DISASM_H

#include <stddef.h>

/* Writes into `out` the text of the instruction starting at mem[pc] and
 * returns its length in bytes (1, 2 or 3). `size` is the size of `mem`. */
unsigned umk_disasm(const unsigned char *mem, unsigned long pc,
                    unsigned long size, char *out, size_t outn);

#endif
