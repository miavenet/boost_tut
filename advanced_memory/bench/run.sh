#!/bin/bash
# Build (via labs/build.sh) and run every bench once. Numbers are machine-specific: re-run on yours.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/labs/build.sh" >/dev/null
echo "machine: $(uname -sm), $(${CXX:-clang++} --version | head -1), flags: -std=c++23 -O2"
for b in "$ROOT"/bench/build/b*; do
    echo; echo "### $(basename "$b")"; "$b"
done
