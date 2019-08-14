#!/bin/sh
set -eu
os="$(uname | tr A-Z- a-z_)"
o_files=""
o_files="$o_files bitvector-ops.o"
o_files="$o_files bitvector.o"
o_files="$o_files buf.o"
o_files="$o_files builtins.o"
o_files="$o_files dump.o"
o_files="$o_files env_unix.o"
o_files="$o_files equalhash.o"
o_files="$o_files flisp.o"
o_files="$o_files flmain.o"
o_files="$o_files hashing.o"
o_files="$o_files htable.o"
o_files="$o_files int2str.o"
o_files="$o_files ios.o"
o_files="$o_files iostream.o"
o_files="$o_files libraries.o"
o_files="$o_files lltinit.o"
o_files="$o_files os_$os.o"
o_files="$o_files os_unix.o"
o_files="$o_files ptrhash.o"
o_files="$o_files random.o"
o_files="$o_files socket.o"
o_files="$o_files string.o"
o_files="$o_files table.o"
o_files="$o_files time_unix.o"
o_files="$o_files utf8.o"
default_cflags="-Wall -O2 -D NDEBUG -D USE_COMPUTED_GOTO -Wextra -std=gnu99 -Wno-strict-aliasing"
default_lflags="-lm"
case "$os" in
darwin)
    default_cc="clang"
    ;;
dragonfly)
    default_cc="gcc"
    ;;
freebsd)
    default_cc="clang"
    ;;
haiku)
    default_cc="gcc"
    default_cflags="-Wall -O2 -D NDEBUG -D USE_COMPUTED_GOTO"
    ;;
linux)
    default_cc="gcc"
    default_cflags="$default_cflags -D _GNU_SOURCE"
    ;;
minix)
    default_cc="clang"
    ;;
netbsd)
    default_cc="gcc"
    ;;
openbsd)
    default_cc="clang"
    ;;
sunos)
    default_cc="gcc"
    default_lflags="$default_lflags -lsocket -lnsl"
    ;;
*)
    echo "Unknown operating system: $os" >&2
    exit 1
    ;;
esac
CC="${CC:-$default_cc}"
CFLAGS="${CFLAGS:-$default_cflags}"
LFLAGS="${LFLAGS:-$default_lflags}"
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

$CC $CFLAGS -c ../c/bitvector-ops.c
$CC $CFLAGS -c ../c/bitvector.c
$CC $CFLAGS -c ../c/buf.c
$CC $CFLAGS -c ../c/builtins.c
$CC $CFLAGS -c ../c/dump.c
$CC $CFLAGS -c ../c/env_unix.c
$CC $CFLAGS -c ../c/equalhash.c
$CC $CFLAGS -c ../c/flisp.c
$CC $CFLAGS -c ../c/hashing.c
$CC $CFLAGS -c ../c/htable.c
$CC $CFLAGS -c ../c/int2str.c
$CC $CFLAGS -c ../c/ios.c
$CC $CFLAGS -c ../c/iostream.c
$CC $CFLAGS -c ../c/libraries.c
$CC $CFLAGS -c ../c/lltinit.c
$CC $CFLAGS -c ../c/os_"$os".c
$CC $CFLAGS -c ../c/os_unix.c
$CC $CFLAGS -c ../c/ptrhash.c
$CC $CFLAGS -c ../c/random.c
$CC $CFLAGS -c ../c/socket.c
$CC $CFLAGS -c ../c/string.c
$CC $CFLAGS -c ../c/table.c
$CC $CFLAGS -c ../c/time_unix.c
$CC $CFLAGS -c ../c/utf8.c

$CC $CFLAGS -c ../c/flmain.c
$CC $LFLAGS -o upscheme $o_files

{ set +x; } 2>/dev/null
cd ../scheme-core
echo "Entering directory '$PWD'"
echo "Creating stage 0 boot file..."
set -x

../"$builddir"/upscheme mkboot0.scm >../scheme-boot/boot_image.h.new
mv ../scheme-boot/boot_image.h.new ../scheme-boot/boot_image.h

{ set +x; } 2>/dev/null
cd ../"$builddir"
echo "Entering directory '$PWD'"
set -x

$CC $CFLAGS -c ../c/flmain.c
$CC $LFLAGS -o upscheme $o_files

{ set +x; } 2>/dev/null
cd ../scheme-core
echo "Entering directory '$PWD'"
echo "Creating stage 1 boot file..."
set -x

../"$builddir"/upscheme mkboot1.scm >../scheme-boot/boot_image.h.new
mv ../scheme-boot/boot_image.h.new ../scheme-boot/boot_image.h

{ set +x; } 2>/dev/null
cd ../"$builddir"
echo "Entering directory '$PWD'"
set -x

$CC $CFLAGS -c ../c/flmain.c
$CC $LFLAGS -o upscheme $o_files

{ set +x; } 2>/dev/null
cd ../scheme-tests
echo "Entering directory '$PWD'"
set -x

../"$builddir"/upscheme unittest.scm
