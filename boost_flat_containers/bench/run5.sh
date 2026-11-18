#!/bin/bash
# Run every bench (both builds) and the capstone variants 5 times, saving each raw run
# under bench/results/runN/. Sequential, normal priority. Then: python3 bench/median.py
set -euo pipefail
cd "$(dirname "$0")"
BASE="clang++ -std=c++23 -O2 -I/opt/homebrew/include"
mkdir -p bin results
for b in lookup_crossover insert_tail bulk_build eytzinger direct_index; do
  $BASE "$b.cpp" -o "bin/$b"; $BASE -DNDEBUG "$b.cpp" -o "bin/$b.ndebug"
done
cd ../labs && bash build.sh >/dev/null && cd ../bench
printf 'risk.reserve(risk.size() + kMaxNewListings);\n' > bin/prepare_reserve.inc
$BASE -DNDEBUG -DPREPARE_FILE='"../bench/bin/prepare_reserve.inc"' ../labs/capstone.cpp -o bin/capstone_reserve
for r in 1 2 3 4 5; do
  d=results/run$r; mkdir -p $d
  uptime > $d/uptime.txt
  for b in lookup_crossover insert_tail bulk_build eytzinger direct_index; do
    "bin/$b" > "$d/$b.txt"; "bin/$b.ndebug" > "$d/$b.ndebug.txt"
  done
  ../labs/bin/capstone > $d/capstone.txt
  bin/capstone_reserve > $d/capstone_reserve.txt
  ../labs/bin/capstone_staged > $d/capstone_staged.txt
  ../labs/bin/capstone_pmr > $d/capstone_pmr.txt 2>&1 || true
  uptime >> $d/uptime.txt
  echo "run $r done"
done
