#!/bin/bash
# Disassemble bench/asm_alloc_paths.cpp (-O2) and print the two functions (lesson 03 C5).
# Symbol names are toolchain-specific: std::__1:: is libc++ (Apple clang here); libstdc++ differs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CXX="${CXX:-clang++}"; BOOST_INC="${BOOST_INC:-/opt/homebrew/include}"
W="$(mktemp -d "${TMPDIR:-/tmp}/asmalloc.XXXXXX")"; trap 'rm -rf "$W"' EXIT
"$CXX" -std=c++23 -O2 -I"$BOOST_INC" -c "$ROOT/bench/asm_alloc_paths.cpp" -o "$W/a.o"
echo "machine: $(uname -sm), $("$CXX" --version | head -1), -std=c++23 -O2"
echo "(stateful_allocate is the function at offset 0; objdump may label it <ltmp0>)"
objdump -d -r --no-show-raw-insn "$W/a.o" | awk '/^Disassembly of section __TEXT,__text|^Disassembly of section .text/{p=1;next} /^Disassembly of section/{p=0} p' | c++filt
