#!/bin/bash
# 04-Q6: the body of prepare(ConfigTable& cfg, RiskTable& risk) in labs/capstone.cpp.
# HIT when the capstone's hot loop makes 0 heap allocations and all lookups are right.
source "$(dirname "$0")/lib.sh"
read_answer > "$WORK/prepare.inc"
"${CXX[@]}" -DNDEBUG -DPREPARE_FILE="\"$WORK/prepare.inc\"" "$ROOT/labs/capstone.cpp" -o "$WORK/cap" 2>/dev/null || exit 1
out="$("$WORK/cap")" || exit 1
printf '%s\n' "$out" | grep -q '^lookups ok' || exit 1
printf '%s\n' "$out" | grep -q '^hot-path heap allocations: 0$'
