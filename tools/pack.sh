#!/bin/sh
# =====================================================================
#  Builds the portable Linux package: build/umk80-linux.tar.gz
#
#  It ships SOURCES, not binaries. The package is tested end to end on Ubuntu
#  22.04 (it builds, passes the four acceptance checks and opens the SDL2
#  window), but a Linux binary would pin a particular glibc and SDL2 and gain
#  nothing here: compiling takes seconds and the package's run.sh does it in a
#  single command.
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

# Drop what makes no sense to distribute.
rm -rf "$STAGE/tools/pack.cmd" "$STAGE/tools/pack.sh"
rm -rf "$STAGE/docs/ref" "$STAGE/docs/pages" "$STAGE/docs/umk_docs.pdf"

( cd build/pack && tar czf ../umk80-linux.tar.gz umk80-linux )

echo "Done: build/umk80-linux.tar.gz"
echo
echo "On the target machine:"
echo "  tar xzf umk80-linux.tar.gz && cd umk80-linux && ./run.sh"
