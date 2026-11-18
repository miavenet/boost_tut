#!/bin/bash
# Run every regression test of labs/hft_mem/hft_mem.hpp (test_hft_mem.cpp) and of hft_slab.hpp,
# hft_book.hpp, hft_handroll.hpp (test_intrusive.cpp, lessons 05-07) and compare each exit status
# with the expected one (0 = passes, 134 = must abort with a message). Exit 0 only if all behave.
#   bash tests/run.sh                         # against labs/hft_mem (what labs/build.sh built)
#   HFT_INC=/some/dir bash tests/run.sh       # against another copy (dir holding hft_mem/*.hpp),
#                                             # e.g. the pre-fix headers, to see each test fail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
CXX="${CXX:-clang++}"
BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
INC="${HFT_INC:-$ROOT/labs}"
W="$(mktemp -d "${TMPDIR:-/tmp}/hfttest.XXXXXX")"; trap 'rm -rf "$W"' EXIT
EXTRA=(); [ -n "${HFT_INC:-}" ] && EXTRA=(-DHFT_TEST_OLD_HEADER)
FL=(-std=c++23 -O2 -Wall -Wextra -I"$BOOST_INC" -I"$INC" ${EXTRA[@]+"${EXTRA[@]}"})
"$CXX" "${FL[@]}" "$HERE/test_hft_mem.cpp" -o "$W/t" || { echo "FAIL compile"; exit 1; }
"$CXX" "${FL[@]}" -DHFT_MEM_CHECKED=0 "$HERE/test_hft_mem.cpp" -o "$W/t0" || { echo "FAIL compile (unchecked)"; exit 1; }
ONLY="${ONLY:-all}"   # ONLY=intrusive: just the lessons 05-07 tests
"$CXX" "${FL[@]}" "$HERE/test_intrusive.cpp" -o "$W/i" || { echo "FAIL compile (intrusive)"; exit 1; }
"$CXX" "${FL[@]}" -DHFT_MEM_CHECKED=0 "$HERE/test_intrusive.cpp" -o "$W/i0" || { echo "FAIL compile (intrusive, unchecked)"; exit 1; }
"$CXX" "${FL[@]}" -DNDEBUG "$HERE/test_intrusive.cpp" -o "$W/ind" || { echo "FAIL compile (intrusive, NDEBUG)"; exit 1; }
bad=0
expect() {  # expect <binary> <mode> <rc> [stderr substring]
    { "$W/$1" "$2" >"$W/out" 2>"$W/err"; } 2>/dev/null; local rc=$?   # hide the shell's "Abort trap" line
    if [ "$rc" -eq "$3" ] && { [ -z "${4:-}" ] || grep -qF "$4" "$W/err"; }; then
        echo "ok   $2 (rc=$rc)"
    else
        echo "FAIL $2 rc=$rc want=$3 :: $(tr '\n' ' ' < "$W/out" | cut -c1-120) $(tr '\n' ' ' < "$W/err" | cut -c1-120)"; bad=1
    fi
}
if [ "$ONLY" = all ]; then
expect t region-overflow 0
expect t alloc-overflow 0
expect t overalign 0
expect t bad-align 134 "alignment is not a power of two"
expect t cross-thread-after-overflow 134 "not from this thread's arena"
expect t own-overflow-ok 0
expect t nested-out-of-order 0
expect t foreign-destroy 134 "destroyed on a thread other than"
expect t warm-no-leak 0
expect t region-size-edges 0
expect t gate-null 0
expect t noexcept-dealloc 0
expect t tripwire-no-bookkeeping 0
expect t zero-byte-edges 0
expect t pin-range 0
expect t mlock-status 0
expect t0 unchecked-null 134 "no thread_arena on this thread"
fi
# lessons 05-07 headers
expect i slab-ctor-throw 0
expect i slab-alloc-fail 0
expect i slab-double-destroy 134 "not live (double destroy?)"
expect i slab-forged-handle 0
expect i0 slab-forged-handle 0
expect i slab-handle-foreign 134 "does not own"
expect i slab-at-range 134 "out of range"
expect i book-churn-full 0
expect i book-dup-id 0
expect i book-bad-px 0
expect i book-mark-state 0
expect ind book-mark-state 0
expect i hr-pool-ctor-throw 0
expect i hr-nocopy 0
expect i hr-link-copy 0
expect i hr-hash-buckets 0
exit $bad
