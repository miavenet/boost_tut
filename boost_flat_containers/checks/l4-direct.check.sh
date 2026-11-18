#!/bin/bash
# 04-Q2: instrument ids are dense 0..N-1. Write
#   const Config* find_cfg(const std::vector<Config>& t, uint32_t id)
# returning the config for id, or nullptr when id is out of range. No search.
source "$(dirname "$0")/lib.sh"
ans="$(read_answer)"
code="$(printf '%s\n' "$ans" | strip_cpp)"      # comments/strings cannot trigger or hide the search ban
# "No search": reject answers that search instead of indexing.
printf '%s' "$code" | grep -qE '(^|[^_[:alnum:]])(find|find_if|lower_bound|upper_bound|equal_range|partition_point|bsearch|binary_search|count)[[:space:]]*\(|(^|[^_[:alnum:]])(for|while)[[:space:]]*\(' && exit 1
printf '%s\n' "$ans" > "$WORK/answer.inc"
cat > "$WORK/t.cpp" <<'CPP'
#include <cstdint>
#include <vector>
struct Config { uint32_t id, lot; };
#include "answer.inc"
int main() {
  std::vector<Config> t(1000);
  for (uint32_t i = 0; i < 1000; ++i) t[i] = {i, 100 + i};
  for (uint32_t i = 0; i < 1000; ++i) { auto* c = find_cfg(t, i); if (!c || c->id != i || c != &t[i]) return 2; }
  if (find_cfg(t, 1000) || find_cfg(t, 4000000000u)) return 3;
  return 0;
}
CPP
"${CXX[@]}" -fsanitize=address "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && ASAN_OPTIONS=detect_leaks=0 "$WORK/t" >/dev/null 2>&1
