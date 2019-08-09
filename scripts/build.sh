#!/bin/sh
set -eu
CC="${CC:-clang}"
CFLAGS="-O2 -falign-functions -Wall -Wno-strict-aliasing -I ../c -D NDEBUG -D USE_COMPUTED_GOTO"
LFLAGS="-lm"
builddir="build-$(uname | tr A-Z- a-z_)-$(uname -m | tr A-Z- a-z_)"
cd "$(dirname "$0")"/..
echo "Entering directory '$PWD'"
set -x
mkdir -p "$builddir"
find "$builddir" -mindepth 1 -delete
{ set +x; } 2>/dev/null
cd "$builddir"
echo "Entering directory '$PWD'"
set -x
$CC $CFLAGS -c ../c/builtins.c
$CC $CFLAGS -c ../c/equalhash.c
$CC $CFLAGS -c ../c/flisp.c
$CC $CFLAGS -c ../c/flmain.c
$CC $CFLAGS -c ../c/iostream.c
$CC $CFLAGS -c ../c/string.c
$CC $CFLAGS -c ../c/table.c
$CC $CFLAGS -c ../c/bitvector-ops.c
$CC $CFLAGS -c ../c/bitvector.c
$CC $CFLAGS -c ../c/dirpath.c
$CC $CFLAGS -c ../c/dump.c
$CC $CFLAGS -c ../c/hashing.c
$CC $CFLAGS -c ../c/htable.c
$CC $CFLAGS -c ../c/int2str.c
$CC $CFLAGS -c ../c/ios.c
$CC $CFLAGS -c ../c/lltinit.c
$CC $CFLAGS -c ../c/ptrhash.c
$CC $CFLAGS -c ../c/random.c
$CC $CFLAGS -c ../c/socket.c
$CC $CFLAGS -c ../c/timefuncs.c
$CC $CFLAGS -c ../c/utf8.c
$CC $LFLAGS -o flisp -lm \
    builtins.o equalhash.o flisp.o flmain.o iostream.o string.o table.o \
    bitvector-ops.o bitvector.o dirpath.o dump.o hashing.o htable.o \
    int2str.o ios.o lltinit.o ptrhash.o random.o socket.o timefuncs.o utf8.o
ln -s ../flisp.boot flisp.boot
{ set +x; } 2>/dev/null
cd ..
echo "Entering directory '$PWD'"
echo "Creating stage 0 boot file..."
set -x
"$builddir"/flisp mkboot0.lsp system.lsp compiler.lsp >flisp.boot.new
mv flisp.boot.new flisp.boot
{ set +x; } 2>/dev/null
echo "Creating stage 1 boot file..."
set -x
"$builddir"/flisp mkboot1.lsp
{ set +x; } 2>/dev/null
cd tests
echo "Entering directory '$PWD'"
../"$builddir"/flisp unittest.lsp
