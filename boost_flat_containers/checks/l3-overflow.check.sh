#!/bin/bash
# 03-Q5: name a type T such that `catch (const T&)` catches the failure of the
# 17th insert into a flat_map backed by static_vector<pair<int,int>,16>.
source "$(dirname "$0")/lib.sh"
T="$(read_answer | tr -d '`' | tr '\n' ' ' | sed -E 's/[;,.[:space:]]+$//' | sed -E 's/^[[:space:]]*catch[[:space:]]*\(//; s/\).*$//; s/&.*$//; s/^[[:space:]]*const[[:space:]]+//; s/[[:space:]]+const[[:space:]]*$//; s/^[[:space:]]+//; s/[[:space:]]+$//')"
[ -n "$T" ] || exit 1
case "$T" in *[!A-Za-z0-9_:]*) exit 1 ;; esac      # a bare (qualified) type name only
cat > "$WORK/t.cpp" <<CPP
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <exception>
#include <new>
#include <stdexcept>
using P = std::pair<int, int>;
using Static = boost::container::flat_map<int, int, std::less<int>, boost::container::static_vector<P, 16>>;
int main() {
  Static t;
  for (int i = 0; i < 16; ++i) t.emplace(i, i);
  try { t.emplace(16, 16); return 2; }
  catch (const $T&) { return t.size() == 16 ? 0 : 3; }
  catch (...) { return 1; }
}
CPP
"${CXX[@]}" "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && "$WORK/t"
