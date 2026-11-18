#!/bin/bash
# 04-Q4: write the statement(s) to run before the session so that inserting
# 1,000,000 distinct keys into boost::unordered_flat_map<uint64_t,uint64_t> h
# never changes h.bucket_count() (no rehash on the hot path).
source "$(dirname "$0")/lib.sh"
read_answer > "$WORK/answer.inc"
cat > "$WORK/t.cpp" <<'CPP'
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
int main() {
  boost::unordered_flat_map<uint64_t, uint64_t> h;
  {
#include "answer.inc"
  }
  if (!h.empty()) return 4;
  auto b = h.bucket_count();
  if (b == 0) return 3;
  for (uint64_t i = 0; i < 1000000; ++i) { h.emplace(i * 0x9E3779B97F4A7C15ULL, i); if (h.bucket_count() != b) return 2; }
  return 0;
}
CPP
"${CXX[@]}" "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && "$WORK/t"
