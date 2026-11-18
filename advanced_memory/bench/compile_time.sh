#!/bin/bash
# Compile-time and size-of-code comparison: Boost.Intrusive vs the hand-rolled header, on two
# small TUs that build the same three structures (bench/ct_boost.cpp, bench/ct_hand.cpp).
# 5 compiles each at -O2; prints every wall time and the median. Also counts lines of code.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
CXX="${CXX:-clang++}"; BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
W="$(mktemp -d "${TMPDIR:-/tmp}/ct.XXXXXX")"; trap 'rm -rf "$W"' EXIT
echo "machine: $(uname -sm), $("$CXX" --version | head -1), flags: -std=c++23 -O2"
for tu in ct_boost ct_hand; do
    ts=()
    for i in 1 2 3 4 5; do
        s=$(python3 -c 'import time;print(time.perf_counter())')
        "$CXX" -std=c++23 -O2 -I"$BOOST_INC" "bench/$tu.cpp" -o "$W/$tu"
        e=$(python3 -c 'import time;print(time.perf_counter())')
        ts+=("$(python3 -c "print(f'{$e-$s:.3f}')")")
    done
    med=$(printf '%s\n' "${ts[@]}" | sort -n | sed -n 3p)
    "$CXX" -std=c++23 -O2 -I"$BOOST_INC" -E "bench/$tu.cpp" -o "$W/$tu.i"
    printf '%-9s compile s: %s  median %s   preprocessed lines: %s\n' "$tu" "${ts[*]}" "$med" "$(wc -l < "$W/$tu.i" | tr -d ' ')"
    "$W/$tu" || true
done
echo
echo "lines you own (hand-rolled):  $(wc -l < labs/hft_mem/hft_handroll.hpp | tr -d ' ') labs/hft_mem/hft_handroll.hpp, $(wc -l < labs/hft_mem/hft_slab.hpp | tr -d ' ') labs/hft_mem/hft_slab.hpp"
echo "Boost.Intrusive headers:      $(cat "$BOOST_INC"/boost/intrusive/*.hpp "$BOOST_INC"/boost/intrusive/detail/*.hpp | wc -l | tr -d ' ') lines in $BOOST_INC/boost/intrusive/{,detail/}*.hpp ($(ls "$BOOST_INC"/boost/intrusive/*.hpp "$BOOST_INC"/boost/intrusive/detail/*.hpp | wc -l | tr -d ' ') files)"
