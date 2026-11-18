#!/bin/bash
# Re-verifies every "does / does not compile" claim in the lessons.
# Each snippet is compiled; we assert the expected outcome and print the first error.
cd "$(dirname "$0")/.."
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
fails=0
claim() {  # claim <compiles|fails> <label> <code>
  printf '%s\n' "$3" > "$W/t.cpp"
  if clang++ -std=c++23 -I/opt/homebrew/include -fsyntax-only "$W/t.cpp" 2>"$W/err"; then got=compiles; else got=fails; fi
  msg="$(grep -m1 'error:' "$W/err" | sed 's/.*error: //')"
  if [ "$got" = "$1" ]; then echo "ok   [$1] $2 ${msg:+:: $msg}"; else echo "FAIL [want $1, got $got] $2"; fails=$((fails+1)); fi
}
H='#include <boost/container/flat_map.hpp>
#include <flat_map>
#include <map>
struct NoMove { int x; NoMove(int v):x(v){} NoMove(const NoMove&)=delete; NoMove(NoMove&&)=delete; };'
claim compiles "Boost flat_map: it->first = 99"        "$H int main(){ boost::container::flat_map<int,int> m{{10,1}}; m.find(10)->first = 99; }"
claim fails    "std::map: it->first = 99"              "$H int main(){ std::map<int,int> m{{10,1}}; m.find(10)->first = 99; }"
claim fails    "std::flat_map: it->first = 99"         "$H int main(){ std::flat_map<int,int> m{{10,1}}; m.find(10)->first = 99; }"
claim fails    "std::flat_map: auto& p = *begin()"     "$H int main(){ std::flat_map<int,double> s{{1,2.0}}; auto& p = *s.begin(); (void)p; }"
claim compiles "std::flat_map: auto p = *begin()"      "$H int main(){ std::flat_map<int,double> s{{1,2.0}}; auto p = *s.begin(); p.second = 7; }"
claim fails    "std::flat_map: reserve()"              "$H int main(){ std::flat_map<int,double> s; s.reserve(10); }"
claim fails    "std::flat_map: extract() on lvalue"    "$H int main(){ std::flat_map<int,double> s; auto c = s.extract(); }"
claim compiles "std::flat_map: std::move(s).extract()" "$H int main(){ std::flat_map<int,double> s; auto c = std::move(s).extract(); s.replace(std::move(c.keys), std::move(c.values)); }"
claim fails    "Boost flat_map: extract(key) node handle" "$H int main(){ boost::container::flat_map<int,int> m{{1,1}}; auto nh = m.extract(1); }"
claim compiles "std::map: extract(key) node handle"    "$H int main(){ std::map<int,int> m{{1,1}}; auto nh = m.extract(1); nh.key() = 5; m.insert(std::move(nh)); }"
claim fails    "Boost flat_map<int, NoMove> emplace"   "$H int main(){ boost::container::flat_map<int,NoMove> m; m.emplace(1, 2); m.emplace(0, 3); }"
claim compiles "std::map<int, NoMove> emplace"         "$H int main(){ std::map<int,NoMove> m; m.emplace(1, 2); m.emplace(0, 3); }"
echo "compile-claim failures: $fails"
exit $((fails > 0))
