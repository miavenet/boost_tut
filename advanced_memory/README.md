# Pooled, hierarchical memory for hot-path containers

Give any hot-path container (Boost.MultiIndex, flat_map, unordered_flat_map, std) a pooled,
NUMA-aware memory supply with a two-line library, then open it up and rebuild a layer.
7 lessons, ~200 min first pass (25–30 min each; 01–04 allocators, 05–07 intrusive containers), then ~5–8 min spaced reviews.
For an experienced C++ developer; numbers measured on an Apple M1 Max (re-run on your box).
It will feel harder than reading: guessing first and recalling from memory feel worse than
re-reading (the fluency illusion), and that effort is what makes it last.

Start (from this directory):
```
bash labs/build.sh                                                     # build labs + benches
python3 drive.py lessons --progress progress.json --today $(date +%F)  # start / resume
python3 drive.py lessons --progress progress.json --today $(date +%F) --reset   # reset (archives old progress)
```
What a session does by default:
- **New material:** up to 6 new concepts per day (`--new-limit 6`, the default), about one lesson.
  When a lesson has fewer than 6, the rest of the budget starts the next lesson; nothing from a
  lesson you are not taught that day is shown. At the default, the first pass takes 7 study days.
- **Confidence:** before each key you mark your confidence (`sure`, `unsure`, `0.8`, `80%` or
  blank). `--no-ask-confidence` turns the prompt off.
- **Sure misses:** a miss marked `sure` (or >= 0.7) prints `!! HIGH-CONFIDENCE MISS`, is re-tested
  first in the same session and before every other due card next session, and stays flagged
  until you hit it on a later day.
- **Misses** come back the next day; hits move through 1/2/7/21-day boxes (about 3 weeks).

Delayed check (once, at least 7 days after you finish lesson 07): 28 unseen items, 2 per
objective (one recall, one transfer to a new setting), about 45 min (estimate; the form's own
header line says 8 minutes, which is the generic template text):
```
python3 drive.py lessons --progress progress.json --today $(date +%F) --assessment
```
Paper version: `assessment/FORM.md` (keys at the end). It stays out of the daily reviews.

No Python? Read `STUDY.md` (paper version, keys at the end).
Benchmarks: `bash bench/run.sh` (results here: `bench/RESULTS.md`).
The library: `labs/hft_mem/hft_mem.hpp`. Contract and sources: `MANIFEST.md`.
Zero-model replay of the whole course (9 study days + the assessment): `bash evals/replay-all.sh`.

Lessons 05–07 (added): intrusive containers over a pool (`hft::slab`), O(1) membership by hook,
Boost.Intrusive vs hand-rolled. ~90 min more (3 × 30). Benches: `bash bench/run_intrusive.sh`
(results: `bench/results_intrusive/RESULTS.md`). Check self-test: `bash checks/selftest.sh`.
