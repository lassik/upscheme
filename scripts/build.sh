#!/bin/sh
set -eu
CFLAGS="-Wall -Wextra -Wno-strict-aliasing -std=gnu99"
CFLAGS="$CFLAGS -O2" # -falign-functions
CFLAGS="$CFLAGS -I ../c -D NDEBUG -D USE_COMPUTED_GOTO"
LFLAGS="-lm"
os="$(uname | tr A-Z- a-z_)"
read -d '' o_files <<EOF || true
    bitvector-ops.o bitvector.o buf.o builtins.o dump.o env_unix.o
    equalhash.o flisp.o flmain.o fs_$os.o fs_unix.o
    hashing.o htable.o int2str.o
    ios.o iostream.o libraries.o lltinit.o ptrhash.o random.o socket.o
    string.o table.o time_unix.o utf8.o
EOF
case "$os" in
darwin)
    default_cc="clang"
    default_cflags="-Wall -Wextra -Wno-strict-aliasing -O2 -falign-functions -std=gnu99"
    ;;
dragonfly)
    default_cc="gcc"
    ;;
freebsd)
    default_cc="clang"
    ;;
haiku)
    default_cc="gcc"
    default_cflags="-Wall"
    ;;
linux)
    default_cc="gcc"
    default_cflags="-std=gnu99 -Wall -Wextra -Wno-strict-aliasing -D _GNU_SOURCE"
    ;;
minix)
    default_cc="clang"
    default_cflags="-std=gnu99 -Wall -Wextra -Wno-strict-aliasing"
    ;;
netbsd)
    default_cc="gcc"
    ;;
openbsd)
    default_cc="clang"
    default_cflags="-Wall"
    ;;
sunos)
    default_cc="gcc"
    ;;
*)
    echo "Unknown operating system: $os" >&2
    exit 1
    ;;
esac
CC="${CC:-$default_cc}"
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
$CC $CFLAGS -c ../c/fs_"$os".c
$CC $CFLAGS -c ../c/fs_unix.c
$CC $CFLAGS -c ../c/hashing.c
$CC $CFLAGS -c ../c/htable.c
$CC $CFLAGS -c ../c/int2str.c
$CC $CFLAGS -c ../c/ios.c
$CC $CFLAGS -c ../c/iostream.c
$CC $CFLAGS -c ../c/libraries.c
$CC $CFLAGS -c ../c/lltinit.c
$CC $CFLAGS -c ../c/ptrhash.c
$CC $CFLAGS -c ../c/random.c
$CC $CFLAGS -c ../c/socket.c
$CC $CFLAGS -c ../c/string.c
$CC $CFLAGS -c ../c/table.c
$CC $CFLAGS -c ../c/time_unix.c
$CC $CFLAGS -c ../c/utf8.c

$CC $CFLAGS -c ../c/flmain.c

echo $o_files
$CC $LFLAGS -o upscheme -lm $o_files

{ set +x; } 2>/dev/null
cd ../scheme-core
echo "Entering directory '$PWD'"
echo "Creating stage 0 boot file..."
set -x
../"$builddir"/upscheme mkboot0.scm system.scm compiler.scm \
   >../scheme-boot/boot_image.h.new
mv ../scheme-boot/boot_image.h.new ../scheme-boot/boot_image.h

{ set +x; } 2>/dev/null
cd ../"$builddir"
echo "Entering directory '$PWD'"
set -x
$CC $CFLAGS -c ../c/flmain.c
$CC $LFLAGS -o upscheme -lm $o_files

{ set +x; } 2>/dev/null
cd ../scheme-core
echo "Entering directory '$PWD'"
echo "Creating stage 1 boot file..."
set -x
../"$builddir"/upscheme mkboot1.scm \
   >../scheme-boot/boot_image.h.new
mv ../scheme-boot/boot_image.h.new ../scheme-boot/boot_image.h

{ set +x; } 2>/dev/null
cd ../"$builddir"
echo "Entering directory '$PWD'"
set -x
$CC $CFLAGS -c ../c/flmain.c
$CC $LFLAGS -o upscheme -lm $o_files

{ set +x; } 2>/dev/null
cd ../scheme-tests
echo "Entering directory '$PWD'"
../"$builddir"/upscheme unittest.scm
