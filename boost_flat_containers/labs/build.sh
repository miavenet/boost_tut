#!/bin/bash
# Build every lab into labs/bin/. Exits non-zero on the first failure.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p bin
CXX="clang++ -std=c++23 -O2 -I/opt/homebrew/include"
b() { echo "  build $1"; shift; "$@"; }
b l1_layout        $CXX l1_layout.cpp -o bin/l1_layout
b l1_assert_on     $CXX l1_ndebug.cpp -o bin/l1_assert_on
b l1_assert_off    $CXX -DNDEBUG l1_ndebug.cpp -o bin/l1_assert_off
b l2_dangling      clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include l2_dangling.cpp -o bin/l2_dangling
b l2_mutable_key   $CXX l2_mutable_key.cpp -o bin/l2_mutable_key
b l2_std_vs_boost  $CXX l2_std_vs_boost.cpp -o bin/l2_std_vs_boost
b l3_sequences     $CXX l3_sequences.cpp -o bin/l3_sequences
b capstone         $CXX -DNDEBUG capstone.cpp -o bin/capstone
b capstone_staged  $CXX -DNDEBUG -DSTAGE_LISTINGS capstone.cpp -o bin/capstone_staged
b capstone_pmr     $CXX -DNDEBUG -DALLOC_SWAP_DEMO capstone.cpp -o bin/capstone_pmr
# The benches compile too (run them with ../bench/run_all.sh).
for f in ../bench/*.cpp; do b "bench/$(basename "$f" .cpp)" $CXX -DNDEBUG -fsyntax-only "$f"; done
echo "all labs built OK"
