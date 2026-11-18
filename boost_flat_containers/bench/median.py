#!/usr/bin/env python3
"""Per-cell median of the 5 raw runs in bench/results/run1..run5 -> bench/results/median/.
Also writes bench/results/median/SPREAD.txt: per file, the median and the worst
(max-min)/median run-to-run spread over all numeric cells. Usage: python3 bench/median.py"""
import re, glob, os, statistics as st
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
out = os.path.join(R, "median"); os.makedirs(out, exist_ok=True)
tok = re.compile(r"\d[\d,]*(?:\.\d+)?")
names = sorted(os.path.basename(p) for p in glob.glob(R + "/run1/*.txt") if "uptime" not in p)
spread_lines = []
for n in names:
    runs = [open(f"{R}/run{r}/{n}").read().splitlines() for r in range(1, 6)]
    if len({len(x) for x in runs}) != 1:
        spread_lines.append(f"{n}: line counts differ across runs (histogram-style file); see raw runs"); 
        open(f"{out}/{n}", "w").write("\n".join(runs[2]) + "\n"); continue
    res, spreads = [], []
    for lines in zip(*runs):
        toks = [list(tok.finditer(l)) for l in lines]
        if len({len(t) for t in toks}) != 1 or not toks[0]:
            res.append(lines[2]); continue
        l0 = lines[0]; new = l0; off = 0
        for i, m in enumerate(toks[0]):
            vals = [float(t[i].group().replace(",", "")) for t in toks]
            med = st.median(vals)
            if max(vals) != min(vals) and med > 0: spreads.append((max(vals) - min(vals)) / med)
            s = m.group()
            if max(vals) == min(vals): continue   # identical in all runs: keep the original text
            dec = len(s.split(".")[1]) if "." in s else 0
            f = f"{med:,.{dec}f}" if "," in s else f"{med:.{dec}f}"
            f = f.rjust(len(s))
            new = new[:m.start() + off] + f + new[m.end() + off:]; off += len(f) - len(s)
        res.append(new)
    open(f"{out}/{n}", "w").write("\n".join(res) + "\n")
    if spreads:
        spread_lines.append(f"{n}: median cell spread {st.median(spreads)*100:.0f}%  p90 {sorted(spreads)[int(.9*len(spreads))-1]*100:.0f}%  max {max(spreads)*100:.0f}%")
    else: spread_lines.append(f"{n}: no spread")
open(f"{out}/SPREAD.txt", "w").write("# (max-min)/median across 5 runs, over all numeric cells in the file\n" + "\n".join(spread_lines) + "\n")
print("\n".join(spread_lines))
