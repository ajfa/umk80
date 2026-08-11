#!/bin/sh
# =====================================================================
#  УМК-80 — portable package for Linux
#
#  One command: ./run.sh
#  Builds if needed and starts the panel. Nothing is installed on the
#  system; everything stays inside this folder.
#
#  Needs: gcc (or clang), make and libsdl2-dev.
# =====================================================================

set -e
cd "$(dirname "$0")"

have() { command -v "$1" >/dev/null 2>&1; }

# Any C compiler will do. GNU make defaults to CC=cc, so if only gcc is
# present it has to be told explicitly or it will not find one.
if have cc; then
    CC=cc
elif have gcc; then
    CC=gcc
elif have clang; then
    CC=clang
else
    echo "No C compiler found."
    echo "On Debian/Ubuntu:  sudo apt install build-essential libsdl2-dev"
    exit 1
fi

if ! have make; then
    echo "'make' is missing."
    echo "On Debian/Ubuntu:  sudo apt install build-essential libsdl2-dev"
    exit 1
fi

if [ ! -f build/umk80 ]; then
    echo "Building with $CC (the first time takes a few seconds)..."
    if ! make -s CC="$CC"; then
        echo
        echo "The build failed. Most likely SDL2 is missing:"
        echo "  sudo apt install libsdl2-dev"
        echo
        echo "Without SDL2 everything except the window still works:"
        echo "  make CC=$CC build/umkcli && ./build/umkcli --rom rom/monitor.bin"
        exit 1
    fi
fi

cat <<'END'

  УМК-80 — Soviet educational trainer (ВЭФ Riga, РР3.059.004)

  HOW TO TRY IT. Follow these six steps in the window that opens. You can
  press the keys on your keyboard or click the drawn keys with the mouse.

    1) Esc                  the СБ button (reset). A dash appears on the
                            leftmost display.

    2) F1  then  0 8 0 0    the П directive (memory) and the address.
       then space           The display shows 0800 and that cell's contents.

    3) Type the program, separating each byte with space:

            3 E  space  A A  space  C 3  space
            0 0  space  0 8  space  Enter

       You have just entered  MVI A,0AAH / JMP 0800H  at 0800.

    4) F3  then  0 8 0 0    the СТ directive (start) and the address.
       then Enter           The program starts looping.

    5) Backspace            the ПР button (interrupt). It stops the
                            program and shows where.

    6) F2  then  A          the РГ directive (registers) and register A.
                            The display shows:   A - AA

       That AA is the value the program had loaded into A. If you see it,
       the emulator is working end to end.

  ALL THE KEYS

    0-9 A-F      hexadecimal keys
    F1 F2 F3     П (memory)    РГ (registers)  СТ (start)
    F4 F5 F6     КС (checksum) ЗК (fill)       ПМ (copy)
    space        parameter separator
    Enter        ВП, end of directive
    Esc          СБ, reset
    Backspace    ПР, interrupt
    F8           ШГ, advance one step
    F9           РБ/ШГ, latches single-step mode
    F10          КМ/ЦК, makes each step one machine cycle

  Close the window to quit.

END

exec ./build/umk80 --rom rom/monitor.bin
