#!/bin/bash
# 01-Q5: give the extra compiler flag(s) that turn Boost.Container's internal
# BOOST_ASSERT checks off in a production build. The answer is passed to clang++.
source "$(dirname "$0")/lib.sh"
flags="$(read_answer | tr -d '`"'"'"'' | tr '\n' ' ' | sed -E 's/-([DUO])[[:space:]]+/-\1/g')"
# Only -D/-U/-O flags are accepted (keeps the check about macros, not files).
for f in $flags; do case "$f" in -D*|-U*|-O*) ;; *) exit 1 ;; esac; done
cat > "$WORK/t.cpp" <<'CPP'
#include <boost/assert.hpp>
#include <boost/container/flat_map.hpp>
int main() {
  boost::container::flat_map<int,int> m{{1,1}};
#if defined(BOOST_ASSERT_IS_VOID) || defined(BOOST_DISABLE_ASSERTS)
  return m.size() == 1 ? 0 : 2;     // assertions compiled out
#else
  return 1;                         // BOOST_ASSERT is still a live assert()
#endif
}
CPP
"${CXX[@]}" $flags "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null && "$WORK/t"
