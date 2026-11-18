#!/bin/bash
# Run every bench (assertions ON and -DNDEBUG) and the capstone variants 5 times,
# saving each raw run under bench/results/run1..run5/, then write the per-cell
# median and the run-to-run spread to bench/results/median/. Takes ~15 minutes.
# Run at NORMAL priority (not nice'd) on an otherwise quiet machine.
set -euo pipefail
cd "$(dirname "$0")"
bash run5.sh
python3 median.py
