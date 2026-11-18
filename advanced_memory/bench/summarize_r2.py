#!/usr/bin/env python3
"""Median and [min-max] over the 5 raw runs in bench/results_r2/ -> Markdown on stdout.
Usage: python3 bench/summarize_r2.py > bench/results_r2/RESULTS.md"""
import glob, os, re, statistics
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results_r2')
ROW = re.compile(r'^(.{3,46}?)\s+n=(\d+)\s+mean=\s*(\d+)\s+p50=\s*(\d+)\s+p99=\s*(\d+)\s+p99\.9=\s*(\d+)\s+max=\s*(\d+) ns(?:\s+faults=(\d+))?')

def files(pat):
    return sorted(glob.glob(os.path.join(D, pat)))

def cell(xs):
    med = statistics.median(xs)
    fmt = (lambda v: f'{v:.2f}') if any(isinstance(x, float) for x in xs) else (lambda v: str(int(v)))
    return f'{fmt(med)} [{fmt(min(xs))}–{fmt(max(xs))}]'

def table(cols, rows):
    print('| ' + ' | '.join(cols) + ' |'); print('|' + '---|' * len(cols))
    for name, vals in rows.items():
        print(f'| {name} | ' + ' | '.join(cell(list(c)) for c in zip(*vals)) + ' |')
    print()

first = files('b1_run*.txt')
print('# Re-measured results (round 2)\n')
print(open(first[0]).readline().strip() + '.')
print(f'{len(first)} runs per program, sequential. Each cell: median [min–max] over the runs. Columns in ns')
print('unless stated; faults = minor page faults inside the measured loop. Raw files: bench/results_r2/*.txt.')
print('The Mac was shared with other jobs (load average is in each raw file), so p99.9 and max move')
print('between runs; trust only large differences that repeat in every run. Regenerate: bash bench/run_r2.sh\n')

print('## bench/b1_containers (N per row in column n)\n')
rows = {}
for f in first:
    for line in open(f):
        m = ROW.match(line.rstrip())
        if m:
            rows.setdefault(m.group(1).strip(), []).append([int(x) if x else 0 for x in m.groups()[2:]])
table(['variant', 'mean', 'p50', 'p99', 'p99.9', 'max', 'faults'], rows)

print('## labs/l1_tail_histogram (200,000 x 256 B, ARENA and MALLOC sections)\n')
H = re.compile(r'mean=(\d+) ns\s+p50=(\d+)\s+p99=(\d+)\s+p99\.9=(\d+)\s+max=(\d+) ns')
rows = {}
for tag, label in [('warm', 'default (warm, cap 256)'), ('nowarm', 'nowarm (cap 256)'), ('nowarm_nocap', 'nowarm nocap')]:
    for f in files(f'l1_tail_{tag}_run*.txt'):
        text = open(f).read()
        faults = [int(x) for x in re.findall(r'page faults in loop: (\d+)', text)]
        stats = [[int(x) for x in g] for g in H.findall(text)]
        rows.setdefault(f'MALLOC, {label} run', []).append(stats[0] + [faults[0]])
        rows.setdefault(f'ARENA, {label}', []).append(stats[1] + [faults[1]])
# MALLOC does not depend on the arena flags: merge its 15 runs into one row.
malloc = [v for k in list(rows) if k.startswith('MALLOC') for v in rows.pop(k)]
rows = {'MALLOC (all 15 runs)': malloc, **rows}
table(['variant', 'mean', 'p50', 'p99', 'p99.9', 'max', 'faults'], rows)

print('## labs/l3_first_touch (64 MiB, 16 KiB pages)\n')
rows = {}
for f in files('l3_first_touch_run*.txt'):
    p = re.findall(r'pass (\d):\s+(\d+) page faults,\s+(\d+) ns per page', open(f).read())
    rows.setdefault('pass 1', []).append([int(p[0][1]), int(p[0][2])])
    rows.setdefault('pass 2', []).append([int(p[1][1]), int(p[1][2])])
table(['pass', 'page faults', 'ns per page'], rows)

print('## labs/l4_hot_cold (1,000,000 orders; ns per order)\n')
rows = {}
for f in files('l4_hot_cold_run*.txt'):
    for name, p50, p99 in re.findall(r'^(.+?)\s+\d+ B/order\s+p50=([\d.]+)\s+p99=([\d.]+)', open(f).read(), re.M):
        rows.setdefault(name.strip(), []).append([float(p50), float(p99)])
table(['layout', 'p50', 'p99'], rows)
if rows:
    names = list(rows)
    ratios = [a[0] / b[0] for a, b in zip(rows[names[0]], rows[names[1]])]
    print(f'Ratio fat/split p50 per run: ' + ', '.join(f'{r:.1f}x' for r in ratios) + '\n')
