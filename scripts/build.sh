#!/bin/sh
set -eu
CC="${CC:-clang}"
CFLAGS="-Wall -Wextra -Wno-strict-aliasing -std=gnu99"
CFLAGS="$CFLAGS -O2" # -falign-functions
CFLAGS="$CFLAGS -I ../c -D NDEBUG -D USE_COMPUTED_GOTO"
LFLAGS="-lm"
os="$(uname | tr A-Z- a-z_)"
builddir="build-$os-$(uname -m | tr A-Z- a-z_)"
cd "$(dirname "$0")"/..
echo "Entering directory '$PWD'"
set -x
mkdir -p "$builddir"
find "$builddir" -mindepth 1 -delete
{ set +x; } 2>/dev/null
cd "$builddir"
echo "Entering directory '$PWD'"
set -x
ln -s ../scheme-boot/flisp.boot flisp.boot
$CC $CFLAGS -c ../c/bitvector-ops.c
$CC $CFLAGS -c ../c/bitvector.c
$CC $CFLAGS -c ../c/builtins.c
$CC $CFLAGS -c ../c/dump.c
$CC $CFLAGS -c ../c/equalhash.c
$CC $CFLAGS -c ../c/flisp.c
$CC $CFLAGS -c ../c/flmain.c
$CC $CFLAGS -c ../c/fs_"$os".c
$CC $CFLAGS -c ../c/fs_unix.c
$CC $CFLAGS -c ../c/hashing.c
$CC $CFLAGS -c ../c/htable.c
$CC $CFLAGS -c ../c/int2str.c
$CC $CFLAGS -c ../c/ios.c
$CC $CFLAGS -c ../c/iostream.c
$CC $CFLAGS -c ../c/lltinit.c
$CC $CFLAGS -c ../c/ptrhash.c
$CC $CFLAGS -c ../c/random.c
$CC $CFLAGS -c ../c/socket.c
$CC $CFLAGS -c ../c/string.c
$CC $CFLAGS -c ../c/table.c
$CC $CFLAGS -c ../c/time_unix.c
$CC $CFLAGS -c ../c/utf8.c
$CC $LFLAGS -o flisp -lm \
        bitvector-ops.o bitvector.o builtins.o dump.o \
        equalhash.o flisp.o flmain.o fs_"$os".o fs_unix.o \
        hashing.o htable.o int2str.o \
        ios.o iostream.o lltinit.o ptrhash.o random.o socket.o \
        string.o table.o time_unix.o utf8.o
{ set +x; } 2>/dev/null
cd ../scheme-core
echo "Entering directory '$PWD'"
echo "Creating stage 0 boot file..."
set -x
../"$builddir"/flisp mkboot0.scm system.scm compiler.scm >flisp.boot.new
mv flisp.boot.new ../scheme-boot/flisp.boot
{ set +x; } 2>/dev/null
echo "Creating stage 1 boot file..."
set -x
../"$builddir"/flisp mkboot1.scm
mv flisp.boot.new ../scheme-boot/flisp.boot
{ set +x; } 2>/dev/null
cd ../scheme-tests
echo "Entering directory '$PWD'"
../"$builddir"/flisp unittest.scm
