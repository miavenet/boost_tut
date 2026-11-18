#!/usr/bin/env bash
# Build every lab and bench. Exits non-zero on the first failure.
# Run from the tutorial directory:  bash labs/build.sh
set -euo pipefail
cd "$(dirname "$0")/.."
CXX=${CXX:-clang++}
FLAGS=(-std=c++23 -O2 -Wall -Wextra -I"${BOOST_INC:-/opt/homebrew/include}")
mkdir -p build
for src in labs/*.cpp bench/*.cpp; do
  out="build/$(basename "${src%.cpp}")"
  echo "build $src -> $out"
  "$CXX" "${FLAGS[@]}" "$src" -o "$out"
done
echo "OK: all labs and benches built into build/"
