/* console.h — pone la consola en UTF-8 en Windows.
 *
 * Todo el texto de este proyecto está en UTF-8 (mensajes en español con
 * acentos, rótulos en cirílico como АДРЕС o ПЗУ). Una consola de Windows
 * arranca en la página de códigos 850 o 437 y esos bytes salen como
 * «ðúð£ðÜ-80». Basta con avisar a la consola de que lo que va a recibir es
 * UTF-8.
 *
 * En POSIX no hace nada: allí la consola ya es UTF-8.
 */
#ifndef UMK80_CONSOLE_H
#define UMK80_CONSOLE_H

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
static void console_utf8(void) { SetConsoleOutputCP(CP_UTF8); }
#else
static void console_utf8(void) { }
#endif

#endif /* UMK80_CONSOLE_H */
