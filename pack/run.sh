#!/bin/sh
# =====================================================================
#  УМК-80 — paquete portable para Linux
#
#  Un solo comando: ./run.sh
#  Compila si hace falta y arranca el panel. No instala nada en el
#  sistema; todo se queda dentro de esta carpeta.
#
#  Necesita: gcc (o clang), make y libsdl2-dev.
# =====================================================================

set -e
cd "$(dirname "$0")"

have() { command -v "$1" >/dev/null 2>&1; }

# Un compilador, el que haya. GNU make trae CC=cc por omisión, así que si
# sólo está gcc hay que decírselo explícitamente o no lo encuentra.
if have cc; then
    CC=cc
elif have gcc; then
    CC=gcc
elif have clang; then
    CC=clang
else
    echo "Falta un compilador de C."
    echo "En Debian/Ubuntu:  sudo apt install build-essential libsdl2-dev"
    exit 1
fi

if ! have make; then
    echo "Falta 'make'."
    echo "En Debian/Ubuntu:  sudo apt install build-essential libsdl2-dev"
    exit 1
fi

if [ ! -f build/umk80 ]; then
    echo "Compilando con $CC (la primera vez tarda unos segundos)..."
    if ! make -s CC="$CC"; then
        echo
        echo "La compilación falló. Lo más probable es que falte SDL2:"
        echo "  sudo apt install libsdl2-dev"
        echo
        echo "Sin SDL2 se puede usar igualmente todo lo que no es la ventana:"
        echo "  make CC=$CC build/umkcli && ./build/umkcli --rom rom/monitor.bin"
        exit 1
    fi
fi

cat <<'FIN'

  УМК-80 — Учебный микропроцессорный комплект  (ВЭФ, РР3.059.004)

  Teclado:  0-9 A-F        teclas hexadecimales
            F1..F6         П  РГ  СТ  КС  ЗК  ПМ
            espacio        separador de parámetros
            Intro          ВП  (fin de directiva)
            Esc            СБ  (сброс)
            Retroceso      ПР  (прерывание)
            F8 / F9 / F10  ШГ / РБ-ШГ / КМ-ЦК

  Prueba rápida, tecleando en el panel:
            СБ, П, 0800, espacio, 3E, espacio, AA, espacio,
            C3, espacio, 00, espacio, 08, espacio, ВП
            luego СТ, 0800, ВП     y después ПР, РГ, A

FIN

exec ./build/umk80 --rom rom/monitor.bin
