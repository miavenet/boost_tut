#!/bin/bash
# Compile bench/asm_hot_ops.cpp to assembly (-O2 -DNDEBUG, and once with asserts on) and write
# instruction / branch / load / store counts plus short excerpts to bench/results_intrusive/asm.md.
# Counts are STATIC (instructions in the function body), not executed counts.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-clang++}"; BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
OUT="${OUT:-$ROOT/bench/results_intrusive}"; mkdir -p "$OUT"
W="$(mktemp -d "${TMPDIR:-/tmp}/asm.XXXXXX")"; trap 'rm -rf "$W"' EXIT
FL=(-std=c++23 -O2 -I"$BOOST_INC")
"$CXX" "${FL[@]}" -DNDEBUG -DHFT_MEM_CHECKED=0 -S "$ROOT/bench/asm_hot_ops.cpp" -o "$W/rel.s"
"$CXX" "${FL[@]}" -DNDEBUG -DHFT_MEM_CHECKED=1 -S "$ROOT/bench/asm_hot_ops.cpp" -o "$W/chk.s"
"$CXX" "${FL[@]}" -DHFT_MEM_CHECKED=0 -S "$ROOT/bench/asm_hot_ops.cpp" -o "$W/dbg.s"
F_LIST="bi_safe_push_back bi_normal_push_back bi_safe_nosize_push_back bi_normal_nosize_push_back hand_push_back idx_push_back
bi_safe_erase bi_normal_erase bi_safe_nosize_erase bi_normal_nosize_erase hand_erase idx_erase bi_safe_is_linked hand_is_linked"
F_POOL="slab_create slab_destroy hand_pool_create hand_pool_destroy boost_pool_malloc boost_pool_free object_pool_construct object_pool_destroy"
HDR='| function | instructions | branches (incl. ret) | loads | stores | calls |
|---|---|---|---|---|---|'
excerpt() { python3 - "$1" "$2" <<'PY'
import sys; sys.path.insert(0, 'bench')   # run from the tutorial root (the script cds there)
import asm_count as a
f = a.functions(sys.argv[1]); print('```'); print(f'{sys.argv[2]}:'); print('\n'.join('    ' + x for x in f[sys.argv[2]])); print('```')
PY
}
cd "$ROOT"
{
echo "# Generated code for the hot operations"
echo
echo "machine: $(uname -sm), $("$CXX" --version | head -1). Source: bench/asm_hot_ops.cpp (each op a noinline"
echo "extern \"C\" function). Regenerate: \`bash bench/asm_intrusive.sh\`. Counts are static instructions in the"
echo "function body (ldp/stp count once though they move two registers), not executed counts."
echo
echo "## Lists, release (-O2 -DNDEBUG)"; echo "$HDR"; python3 bench/asm_count.py "$W/rel.s" $F_LIST
echo
echo "## Lists, asserts ON (-O2, no NDEBUG): what Boost safe_link adds in a debug build"; echo "$HDR"
python3 bench/asm_count.py "$W/dbg.s" bi_safe_push_back bi_normal_push_back bi_safe_erase bi_normal_erase
echo
echo "## Pools, release (-O2 -DNDEBUG), hft::slab with HFT_MEM_CHECKED=0"; echo "$HDR"; python3 bench/asm_count.py "$W/rel.s" $F_POOL
echo
echo "## hft::slab with HFT_MEM_CHECKED=1 (the library default)"; echo "$HDR"; python3 bench/asm_count.py "$W/chk.s" slab_create slab_destroy
echo
echo "## Excerpts (release)"
for f in bi_safe_erase bi_normal_erase bi_normal_nosize_erase hand_erase bi_safe_push_back bi_safe_nosize_push_back hand_push_back bi_safe_is_linked idx_erase slab_destroy hand_pool_destroy boost_pool_free object_pool_destroy; do excerpt "$W/rel.s" "$f"; done
echo
echo "## Excerpt, asserts ON: bi_safe_push_back"
excerpt "$W/dbg.s" bi_safe_push_back
echo
echo "## Excerpt, HFT_MEM_CHECKED=1: slab_destroy"
excerpt "$W/chk.s" slab_destroy
} > "$OUT/asm.md"
echo "wrote $OUT/asm.md"
