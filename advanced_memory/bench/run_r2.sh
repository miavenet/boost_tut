#!/bin/bash
# Re-measurement for lessons 01-04 (second round): 5 raw runs of each program the
# lessons quote, saved under bench/results_r2/ (bench/results/ is left untouched). Then
# summarize: python3 bench/summarize_r2.py > bench/results_r2/RESULTS.md
# Runs are sequential (they are timing measurements) and at normal priority.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/bench/results_r2"; mkdir -p "$OUT"
bash "$ROOT/labs/build.sh" >/dev/null
HDR="machine: $(uname -sm) ($(sysctl -n machdep.cpu.brand_string 2>/dev/null || uname -p)), $(${CXX:-clang++} --version | head -1), flags: -std=c++23 -O2, shared machine (not an isolated core)"
for i in 1 2 3 4 5; do
    { echo "$HDR"; echo "run $i, $(date '+%F %T'), load: $(uptime | sed 's/.*load averages*: //')"; "$ROOT/bench/build/b1_containers"; } > "$OUT/b1_run$i.txt"
    for mode in "" "nowarm" "nowarm nocap"; do
        tag="${mode// /_}"; tag="${tag:-warm}"
        { echo "$HDR"; echo "run $i: labs/build/l1_tail_histogram $mode"; "$ROOT/labs/build/l1_tail_histogram" $mode; } > "$OUT/l1_tail_${tag}_run$i.txt"
    done
    { echo "$HDR"; echo "run $i: labs/build/l3_first_touch"; "$ROOT/labs/build/l3_first_touch"; } > "$OUT/l3_first_touch_run$i.txt"
    { echo "$HDR"; echo "run $i: labs/build/l4_hot_cold"; "$ROOT/labs/build/l4_hot_cold"; } > "$OUT/l4_hot_cold_run$i.txt"
    echo "run $i done"
done
