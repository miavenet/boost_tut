#!/usr/bin/env python3
"""Median and spread of every row across bench/results_intrusive/run*.txt -> Markdown on stdout.
The spread column is min..max of that percentile over the runs: if two rows' spreads overlap,
treat their difference as run-to-run noise on this shared machine."""
import glob, re, statistics, sys
from collections import OrderedDict
ROW = re.compile(r'^(.{3,46}?)\s+n=(\d+)\s+mean=\s*(\d+)\s+p50=\s*(\d+)\s+p99=\s*(\d+)\s+p99\.9=\s*(\d+)\s+max=\s*(\d+) ns(?:\s+faults=(\d+))?')
files = sorted(sys.argv[1:] or glob.glob('bench/results_intrusive/run*.txt'))
rows, loads = OrderedDict(), []
machine = ''
for f in files:
    for line in open(f):
        line = line.rstrip()
        if line.startswith('machine:'): machine = line
        if line.startswith('load:'): loads.append(line[5:].strip())
        m = ROW.match(line)
        if m:
            rows.setdefault(m.group(1).strip(), []).append([int(x) if x else 0 for x in m.groups()[1:]])
print('# Measured: intrusive containers over a pool (lessons 05-07)\n')
print(f'{machine}. N per row in column n. Benches: bench/b4_membership.cpp, b5_book.cpp, b6_handroll.cpp;')
print('every row ran in its own process. Shared machine, not an isolated core: load average at the start')
print('of each run was: ' + ' | '.join(loads) + '.')
print('The timer step here is about 41 ns, so p50 values of 0/41/42/83 ns are quantised.')
print(f'Median of {len(files)} runs; "range" = min..max over the runs. Re-run: bash bench/run_intrusive.sh\n')
print('| variant | n | mean | p50 | p99 | p99.9 | max | mean range | p50 range | p99 range | p99.9 range | max range | faults |')
print('|---|---|---|---|---|---|---|---|---|---|---|---|---|')
for name, runs in rows.items():
    cols = list(zip(*runs))
    med = [int(statistics.median(c)) for c in cols]
    rng = lambda i: f'{min(cols[i])}..{max(cols[i])}'
    print(f'| {name} | {med[0]} | {med[1]} | {med[2]} | {med[3]} | {med[4]} | {med[5]} | {rng(1)} | {rng(2)} | {rng(3)} | {rng(4)} | {rng(5)} | {med[6]} |')
