#!/bin/bash
# Build every lab (and compile every bench). Exits non-zero on the first failure.
# Usage: bash labs/build.sh        (from the tutorial directory or anywhere)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
CXX="${CXX:-clang++}"
BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
BOOST_LIB="${BOOST_LIB:-/opt/homebrew/lib}"
FLAGS=(-std=c++23 -O2 -Wall -Wextra -I"$BOOST_INC" -I"$HERE")
mkdir -p "$HERE/build" "$ROOT/bench/build"

build() {  # build <out> <src> [extra flags...]
    local out="$1" src="$2"; shift 2
    printf '  %-28s ' "$(basename "$out")"
    "$CXX" "${FLAGS[@]}" "$src" -o "$out" "$@"
    echo ok
}

echo "labs:"
for src in "$HERE"/l*.cpp; do
    name="$(basename "$src" .cpp)"
    extra=()
    case "$name" in
        l2_boost_pmr)  extra=(-L"$BOOST_LIB" -lboost_container -Wl,-rpath,"$BOOST_LIB") ;;
        l3_numa_linux) [[ "$(uname)" == Linux ]] && extra=(-lnuma) ;;
    esac
    build "$HERE/build/$name" "$src" ${extra[@]+"${extra[@]}"}
done
build "$HERE/build/l4_capstone_tripwire" "$HERE/l4_capstone.cpp" -DHFT_MEM_TRIPWIRE=1

echo "library regression tests (run: bash tests/run.sh, also run by checks/selftest.sh):"
build "$HERE/build/test_hft_mem" "$ROOT/tests/test_hft_mem.cpp"
build "$HERE/build/test_hft_mem_unchecked" "$ROOT/tests/test_hft_mem.cpp" -DHFT_MEM_CHECKED=0

echo "bench:"
for src in "$ROOT"/bench/b*.cpp; do
    build "$ROOT/bench/build/$(basename "$src" .cpp)" "$src"
done
# --- lessons 05-07 (intrusive containers over a pool) ---------------------------------------
echo "lessons 05-07 extra builds:"
build "$HERE/build/l6_traps_ndebug" "$HERE/l6_traps.cpp" -DNDEBUG
build "$HERE/build/l5_membership_ndebug" "$HERE/l5_membership.cpp" -DNDEBUG
build "$HERE/build/test_intrusive" "$ROOT/tests/test_intrusive.cpp"
build "$ROOT/bench/build/b6_handroll_asserts" "$ROOT/bench/b6_handroll.cpp" -DBENCH_ASSERTS
build "$ROOT/bench/build/asm_hot_ops.o" "$ROOT/bench/asm_hot_ops.cpp" -c
build "$ROOT/bench/build/ct_boost" "$ROOT/bench/ct_boost.cpp"
build "$ROOT/bench/build/ct_hand" "$ROOT/bench/ct_hand.cpp"
echo "all built"
