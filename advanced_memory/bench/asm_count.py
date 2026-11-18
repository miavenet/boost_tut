#!/usr/bin/env python3
"""Count instructions / branches / loads / stores per function in a clang -S (arm64 or x86-64)
listing. Usage: asm_count.py file.s [func ...]  -> Markdown table rows on stdout.
A function's body runs from its `_name:` (or `name:`) label to the next .cfi_endproc.
Counted on the static listing: these are instructions in the function, NOT executed counts;
a call (bl / call) to an out-of-line helper is one instruction here however long the helper is."""
import re, sys

BR = re.compile(r'^(b|b\.\w+|bl|blr|br|cbz|cbnz|tbz|tbnz|ret|j\w*|call\w*|ret\w*)$')
LD = re.compile(r'^(ldr\w*|ldp\w*|ldur\w*|ldar\w*|ldx\w*)$')
ST = re.compile(r'^(str\w*|stp\w*|stur\w*|stlr\w*)$')

def functions(path):
    funcs, cur = {}, None
    for line in open(path):
        s = line.rstrip('\n')
        m = re.match(r'^_?([A-Za-z_][\w.$]*):', s)
        if m and not s.startswith('\t') and not m.group(1).startswith(('L', 'l_', 'ltmp')):
            cur = m.group(1); funcs[cur] = []; continue
        if cur is None: continue
        t = s.split(';')[0].split('//')[0].strip()      # drop trailing comments first
        if t.startswith('.cfi_endproc'): cur = None; continue
        if not t or t.startswith(('.', '#')) or t.endswith(':'): continue
        funcs[cur].append(t)
    return funcs

def count(body):
    n = len(body); br = ld = st = 0; calls = []
    for ins in body:
        op = ins.split()[0]
        if BR.match(op): br += 1
        if op in ('bl', 'call', 'callq'): calls.append(ins.split()[-1])
        if LD.match(op): ld += 1
        if ST.match(op): st += 1
    return n, br, ld, st, calls

if __name__ == '__main__':
    f = functions(sys.argv[1])
    names = sys.argv[2:] or sorted(f)
    for name in names:
        if name not in f:
            print(f'| {name} | (not found) | | | | |'); continue
        n, br, ld, st, calls = count(f[name])
        c = ', '.join(sorted(set(x.lstrip('_') for x in calls))) or '-'
        print(f'| {name} | {n} | {br} | {ld} | {st} | {c} |')
