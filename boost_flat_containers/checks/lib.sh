#!/bin/bash
# Shared helpers for checks/*.check.sh. The learner's answer arrives on stdin.
# Exit 0 = HIT, non-zero = MISS. Each check compiles and RUNS the answer.
set -u
CHECKS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$CHECKS_DIR/.." && pwd)"
CXX=(clang++ -std=c++23 -O2 -I/opt/homebrew/include)
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
# Read stdin, drop Markdown code fences, keep everything else verbatim.
read_answer() { sed -E '/^[[:space:]]*(```|~~~)/d'; }
# Strip C++ comments and string/char literals (so words inside them cannot fool a check).
strip_cpp() { python3 -I -c '
import re,sys
s=sys.stdin.read()
s=re.sub(r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|\x27(?:\\.|[^\x27\\])*\x27"," ",s,flags=re.S)
sys.stdout.write(s)'; }
