#!/bin/bash
# Lessons 05-07 benches: 5 runs, every row/design/variant in its OWN process, raw output to
# bench/results_intrusive/runN.txt, medians + spread to bench/results_intrusive/RESULTS.md.
# Also regenerates asm.md and compile_time.txt. Writes ONLY under $OUT (default bench/results_intrusive/).
#   OUT=bench/results_intrusive_r2 OLD_HDR=/path/to/older/headers bash bench/run_intrusive.sh
#   = the re-measure: also runs b5 A/A2 and b6 hand/slab pools built against the pre-fix
#     headers (a directory holding hft_mem/*.hpp) in the same session (rows prefixed "pre-fix"), to isolate what the fixes changed.
# This Mac is shared: check `uptime` and `tmutil currentphase` first; numbers are noisy.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
OUT="${OUT:-bench/results_intrusive}"; mkdir -p "$OUT"
RUNS="${RUNS:-5}"
bash labs/build.sh >/dev/null
B=bench/build
if [ -n "${OLD_HDR:-}" ]; then   # same sources, pre-fix headers (sources include ../labs/hft_mem: rewrite that)
    for b in b5_book b6_handroll; do
        sed 's|"../labs/hft_mem/|"hft_mem/|' bench/$b.cpp > "$B/${b}_prefix.cpp"
        ${CXX:-clang++} -std=c++23 -O2 -Wall -Wextra -I/opt/homebrew/include -I"$OLD_HDR" -Ibench "$B/${b}_prefix.cpp" -o "$B/${b}_prefix"
    done
fi
for r in $(seq 1 "$RUNS"); do
    f="$OUT/run$r.txt"
    {
        echo "machine: $(uname -sm) $(sysctl -n machdep.cpu.brand_string 2>/dev/null || true), $(${CXX:-clang++} --version | head -1), flags: -std=c++23 -O2 -DNDEBUG"
        echo "load: $(uptime | sed 's/.*load averages*: //')   time machine: $(tmutil currentphase 2>/dev/null || echo n/a)"
        for row in hook bits enum uset flatset mi list t_hook t_uset t_enum t_list scan_hooked scan_lean; do
            "$B/b4_membership" "$row" | grep ' n=' ; done
        "$B/b6_handroll" floor | grep ' n='
        for d in A A2 B C D; do "$B/b5_book" "$d" | grep ' n=' ; done
        for v in dlist idx safe normal safe_nosize normal_nosize slist; do "$B/b6_handroll" lists "$v" | grep ' n='; done
        for v in hash uset set flatmap flatmap2x; do "$B/b6_handroll" ids "$v" | grep -E ' n=|bucket_count'; done
        for v in hand slab pool pool_ordered object_pool new; do "$B/b6_handroll" pools "$v" | grep ' n='; done
        for v in safe normal; do "$B/b6_handroll_asserts" lists "$v" | grep ' n=' | sed 's/^bi::list/ASSERTS bi::list/'; done
        if [ -n "${OLD_HDR:-}" ]; then
            for d in A A2; do "$B/b5_book_prefix" "$d" | grep ' n=' | sed 's/^/pre-fix /'; done
            for v in hand slab; do "$B/b6_handroll_prefix" pools "$v" | grep ' n=' | sed 's/^/pre-fix /'; done
        fi
    } > "$f"
    echo "wrote $f"
done
OUT="$ROOT/$OUT" bash bench/asm_intrusive.sh
bash bench/compile_time.sh > "$OUT/compile_time.txt"
python3 bench/summarize_intrusive.py "$OUT"/run*.txt > "$OUT/RESULTS.md"
echo "wrote $OUT/RESULTS.md"
