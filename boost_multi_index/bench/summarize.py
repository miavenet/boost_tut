#!/usr/bin/env python3
"""Per-metric median of p50/p99 over the 5 saved runs, with min-max spread. Usage: python3 bench/summarize.py"""
import re, glob, statistics as st, os, sys
d = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'results')
pat = re.compile(r'^(.*?)\s+n=\d+\s+mean=\s*([\d.]+)\s+p50=\s*([\d.]+)\s+p99=\s*([\d.]+)')
pat2 = re.compile(r'^(\s*\[next.*?\]\s)?(.*?)\s+n=\d+\s+(?:mean=\s*([\d.]+)\s+)?p50=\s*([\d.]+)\s+p99=\s*([\d.]+)')
for name in ['bench_lookup','bench_modify','bench_rehash','bench_scatter','bench_ordered','capstone']:
    runs=[];xs=[]
    for f in sorted(glob.glob(f'{d}/{name}.run*.txt')):
        sect='';rows={};extra={}
        for line in open(f):
            line=line.rstrip('\n')
            if line.startswith('--') or line.startswith('MIStore') or line.startswith('FastStore'): sect=line
            mm=re.search(r'max=\s*([\d.]+)',line); ov=re.search(r'>100us:(\d+)',line)
            m=pat2.match(line)
            if m:
                key=(sect[:60], m.group(2).strip())
                rows[key]=(float(m.group(4)),float(m.group(5)))
                extra[key]=(float(mm.group(1)) if mm else 0, int(ov.group(1)) if ov else -1)
        runs.append(rows); xs.append(extra)
    print(f'=== {name}: {len(runs)} runs; per-metric median [min..max] of p50 / p99 (ns)')
    cur=None
    for key in runs[0]:
        if key[0]!=cur: cur=key[0]; print(cur)
        p50=[r[key][0] for r in runs]; p99=[r[key][1] for r in runs]
        print(f'  {key[1]:<42} p50 {st.median(p50):>9.1f} [{min(p50):.1f}..{max(p50):.1f}]   p99 {st.median(p99):>9.1f} [{min(p99):.1f}..{max(p99):.1f}]'+(f'   max-of-runs {max(x[key][0] for x in xs):.0f} ns, >100us per run {[x[key][1] for x in xs]}' if name in ('bench_rehash','bench_ordered') else ''))

# bench_rehash detail: median over runs of mean / p99.9 / max (ns) and >100us counts
for bn in ('bench_rehash','bench_ordered'):
  if os.path.exists(f'{d}/{bn}.run1.txt'):
    print(f'=== {bn} detail: median [min..max] over runs of mean, p99.9, max (ns)')
    rx = re.compile(r'^(.*?)\s+n=\d+\s+mean=\s*([\d.]+)\s+p50=\s*[\d.]+\s+p99=\s*[\d.]+\s+p99\.9=\s*([\d.]+)\s+max=\s*([\d.]+)')
    per={}
    for f in sorted(glob.glob(f'{d}/{bn}.run*.txt')):
        for line in open(f):
            m=rx.match(line)
            if m: per.setdefault(m.group(1).strip(),[]).append(tuple(float(m.group(i)) for i in (2,3,4)))
    for k,v in per.items():
        cols=[[r[i] for r in v] for i in range(3)]
        print(f'  {k:<42} '+'  '.join(f'{n} {st.median(c):.0f} [{min(c):.0f}..{max(c):.0f}]' for n,c in zip(('mean','p99.9','max'),cols)))
