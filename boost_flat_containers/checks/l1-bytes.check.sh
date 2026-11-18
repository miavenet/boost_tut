#!/bin/bash
# 01-Q3: bytes moved by inserting a new smallest key into a 10,000-element
# flat_map<uint64_t, uint64_t> (sizeof(value_type) = 16) with spare capacity.
# HIT: the answer states 160000 (commas/underscores allowed; "160 kB"/"160 KB"/"156.25 KiB"
# too) and every other number is part of the worked arithmetic (10000, 16).
# MISS: no 160000, "bits", negations ("not 160000"), or a rival total ("or 80000").
source "$(dirname "$0")/lib.sh"
read_answer | python3 -I -c '
import re,sys
t=sys.stdin.read().lower().replace(",","").replace("_","")
if re.search(r"\bbits?\b|\bnot\b|n\x27t\b|\bwrong\b|\binstead\b|\bno\b",t): sys.exit(1)
vals=[]
for m in re.finditer(r"(\d+(?:\.\d+)?)\s*(kib|kb|k)?\b",t):
    v=float(m.group(1)); u=m.group(2)
    if u=="kib": v*=1024
    elif u: v*=1000
    vals.append(v)
if 160000 not in vals: sys.exit(1)
sys.exit(0 if all(v in (160000,10000,16) for v in vals) else 1)'
