#!/usr/bin/env python3
"""Median of each percentile across bench/results/run*.txt -> a Markdown table on stdout."""
import glob, re, statistics, sys
from collections import OrderedDict
ROW = re.compile(r'^(.{3,46}?)\s+n=(\d+)\s+mean=\s*(\d+)\s+p50=\s*(\d+)\s+p99=\s*(\d+)\s+p99\.9=\s*(\d+)\s+max=\s*(\d+) ns(?:\s+faults=(\d+))?')
rows = OrderedDict()
files = sorted(glob.glob(sys.argv[1] if len(sys.argv) > 1 else 'bench/results/run*.txt'))
for f in files:
    for line in open(f):
        m = ROW.match(line.rstrip())
        if m:
            rows.setdefault(m.group(1).strip(), []).append([int(x) if x else 0 for x in m.groups()[1:]])
machine = next((l.strip() for l in open(files[0]) if l.startswith('machine:')), 'machine: ?') if files else ''
print('# Measured bench results\n')
print(f'{machine}. Benches: bench/b1_containers.cpp, b2_alloc_flavors.cpp, b3_locality.cpp; N per row in column n.')
print('Not an isolated core: other jobs shared this Mac, so p99.9/max move between runs; trust only')
print('differences that are large and repeat. Re-run on your own box: bash bench/run.sh\n')
print(f'Median of {len(files)} runs. Columns in ns; faults = minor page faults inside the measured loop.\n')
print('| variant | n | mean | p50 | p99 | p99.9 | max | faults |')
print('|---|---|---|---|---|---|---|---|')
for name, runs in rows.items():
    med = [int(statistics.median(col)) for col in zip(*runs)]
    print(f'| {name} | ' + ' | '.join(str(x) for x in med) + ' |')
