#!/bin/sh
set -eu
cd "$(dirname "$0")"/..
echo "Entering directory '$PWD'"
set -x
find . -name "[^.]*.[ch]" | sort | xargs clang-format -verbose -i
