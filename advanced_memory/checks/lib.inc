# lib.inc — shared helpers for checks/*.sh (sourced, not run).
# A check reads the learner's answer on stdin, pastes it into a small C++ program, compiles it
# against labs/hft_mem, runs it, and exits 0 only if the program behaves as the key says.
set -u
CHECK_DIR="$(cd "$(dirname "$0")" && pwd)"
TUT_DIR="$(dirname "$CHECK_DIR")"
CXX="${CXX:-clang++}"
BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/amcheck.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# read_answer: stdin -> $WORK/answer.txt with ``` fence lines removed; fails on empty answer.
read_answer() {
    grep -v '^[[:space:]]*```' > "$WORK/answer.txt" || true
    if ! grep -q '[^[:space:]]' "$WORK/answer.txt"; then exit 1; fi
}

# compile_run <source> : compile quietly, run, return the program's exit status.
compile_run() {
    "$CXX" -std=c++23 -O1 -I"$BOOST_INC" -I"$TUT_DIR/labs" "$1" -o "$WORK/prog" 2>"$WORK/cc.log" || {
        [ -n "${CHECK_VERBOSE:-}" ] && cat "$WORK/cc.log" >&2
        return 1
    }
    "$WORK/prog"
}
