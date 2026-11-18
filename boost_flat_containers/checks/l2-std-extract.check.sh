#!/bin/bash
# 02-Q6: with std::flat_map<int,double> s already filled, write statements that
# take out its two containers, set every mapped value to 0.0, and put them back.
source "$(dirname "$0")/lib.sh"
read_answer > "$WORK/answer.inc"
# The card demands extract + replace: require both calls in real code (not comments/strings).
strip_cpp < "$WORK/answer.inc" > "$WORK/code.txt"
grep -qE '\.extract[[:space:]]*\(' "$WORK/code.txt" || exit 1
grep -qE '\.replace[[:space:]]*\(' "$WORK/code.txt" || exit 1
cat > "$WORK/t.cpp" <<'CPP'
#include <flat_map>
#include <vector>
int main() {
  std::flat_map<int, double> s{{1, 1.5}, {2, 2.5}, {3, 3.5}};
  const double* before = &s.values()[0];
  {
#include "answer.inc"
  }
  if (s.size() != 3 || s.keys() != std::vector<int>{1, 2, 3}) return 2;
  for (double v : s.values()) if (v != 0.0) return 3;
  return &s.values()[0] == before ? 0 : 4;      // moved back, not copied into new storage
}
CPP
"${CXX[@]}" "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && "$WORK/t"
