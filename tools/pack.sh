#!/bin/sh
# =====================================================================
#  Arma el paquete portable de Linux: build/umk80-linux.tar.gz
#
#  Va con FUENTES, no con binarios. El paquete está probado de punta a punta
#  en Ubuntu 22.04 (compila, pasa las cuatro comprobaciones y abre la ventana
#  SDL2), pero un binario de Linux ata a una versión concreta de glibc y de
#  SDL2, y aquí no gana nada: compilar son unos segundos y el run.sh del
#  paquete lo hace en un solo comando.
# =====================================================================
set -e
cd "$(dirname "$0")/.."

STAGE=build/pack/umk80-linux
rm -rf "$STAGE"
mkdir -p "$STAGE"

for d in core frontend cli tools rom tests; do
    mkdir -p "$STAGE/$d"
    cp -r "$d/." "$STAGE/$d/"
done
mkdir -p "$STAGE/docs"

cp Makefile CMakeLists.txt README.md PLAN.md DESCONOCIDOS.md "$STAGE/"
cp docs/FUENTES.md "$STAGE/docs/"
cp pack/run.sh "$STAGE/"
chmod +x "$STAGE/run.sh"

# Fuera lo que no tiene sentido distribuir.
rm -rf "$STAGE/tools/pack.cmd" "$STAGE/tools/pack.sh"
rm -rf "$STAGE/docs/ref" "$STAGE/docs/pages" "$STAGE/docs/umk_docs.pdf"

( cd build/pack && tar czf ../umk80-linux.tar.gz umk80-linux )

echo "Listo: build/umk80-linux.tar.gz"
echo
echo "En la máquina de destino:"
echo "  tar xzf umk80-linux.tar.gz && cd umk80-linux && ./run.sh"
