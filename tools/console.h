/* console.h — puts the console into UTF-8 on Windows.
 *
 * All the text in this project is UTF-8, including Cyrillic labels such as
 * АДРЕС or ПЗУ. A Windows console starts in code page 850 or 437 and those
 * bytes come out as «ðúð£ðÜ-80». Telling the console what it is about to
 * receive is enough.
 *
 * On POSIX it does nothing: the console is already UTF-8 there.
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
