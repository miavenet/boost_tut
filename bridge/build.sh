#!/bin/bash
# Compile and run both bridge programs. Exits non-zero on any compile or run failure.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
CXX="${CXX:-clang++}"
mkdir -p build
for n in bridge_multi_index bridge_flat; do
    echo "== $n"
    "$CXX" -std=c++23 -O2 -DNDEBUG -I/opt/homebrew/include -I../advanced_memory/labs "$n.cpp" -o "build/$n"
    "./build/$n"
done
echo "bridge: all ok"
