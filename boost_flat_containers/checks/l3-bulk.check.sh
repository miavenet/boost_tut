#!/bin/bash
# 03-Q2: write FM build(std::vector<std::pair<uint64_t,uint64_t>> v) returning a
# flat_map of v's pairs (random order, unique keys) without inserting one by one.
# Graded RELATIVE to one-by-one insert timed in the same process (hardware-independent):
# the answer must be at least 4x faster than inserting the 100,000 pairs one at a time.
source "$(dirname "$0")/lib.sh"
read_answer > "$WORK/answer.inc"
cat > "$WORK/t.cpp" <<'CPP'
#include <boost/container/flat_map.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>
using FM = boost::container::flat_map<uint64_t, uint64_t>;
namespace bc = boost::container;
#include "answer.inc"
int main() {
  std::vector<std::pair<uint64_t, uint64_t>> v(100000);
  uint64_t x = 88172645463325252ULL;
  for (size_t i = 0; i < v.size(); ++i) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; v[i] = {x, i}; }
  auto ref = v;
  double base;
  { FM b; auto t1 = std::chrono::steady_clock::now(); for (auto& p : v) b.insert(p);
    base = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count(); }
  auto t0 = std::chrono::steady_clock::now();
  FM m = build(v);
  double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  if (m.size() != ref.size()) return 2;
  for (auto& p : ref) { auto it = m.find(p.first); if (it == m.end() || it->second != p.second) return 3; }
  return s * 4 < base ? 0 : 4;
}
CPP
"${CXX[@]}" -DNDEBUG "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && "$WORK/t"
