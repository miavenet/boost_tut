#!/bin/bash
# Card 06-Q3: ONE statement that empties the level `lvl` AND hands every order back to `pool`.
# Guard: exactly one top-level statement and no loop (a hand-written while/for loop that pops and
# destroys also empties the level, but it is not the one-statement disposer the card asks for).
. "$(dirname "$0")/lib.inc"
read_answer
python3 - "$WORK/answer.txt" <<'GUARD' || exit 1
import re, sys
s = open(sys.argv[1]).read()
s = re.sub(r'/\*.*?\*/', '', re.sub(r'//[^\n]*', '', s), flags=re.S)
if re.search(r'\b(for|while|do|goto)\b', s): sys.exit(1)
depth = top = 0
for ch in s:
    if ch in '({[': depth += 1
    elif ch in ')}]': depth -= 1
    elif ch == ';' and depth == 0: top += 1
sys.exit(0 if top == 1 and s.strip().endswith(';') else 1)
GUARD
{
cat <<'CPP'
#include "hft_mem/hft_slab.hpp"
#include <boost/intrusive/list.hpp>
#include <cstdlib>
namespace bi = boost::intrusive;
struct Order { long id; bi::list_member_hook<> h; };
using Level = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::h>>;
int main() {
    hft::thread_arena arena;
    hft::slab<Order> pool(100);
    {
        Level lvl;
        for (long i = 0; i < 50; ++i) lvl.push_back(*pool.create(Order{i, {}}));
CPP
cat "$WORK/answer.txt"
cat <<'CPP'

        if (!lvl.empty() || pool.size() != 0) std::_Exit(1);   // orders leaked from the slab
    }
    return pool.size() == 0 ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
