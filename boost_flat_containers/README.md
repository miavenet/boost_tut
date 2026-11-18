# Boost flat containers for hot-path lookup tables

Choose between `boost::container::flat_map`, `std::map`, `std::unordered_map`,
C++23 `std::flat_map`, `boost::unordered_flat_map`, direct indexing and Eytzinger,
and predict lookup/insert/erase latency **including the tail**. You will hit a
dangling reference under ASan and remove a ~0.2 ms spike from an order-entry loop.

**Time (estimate, never timed with a learner):** ~105 min first pass over **5 study
dates** (one lesson per sitting, 25–30 min; lesson 4's capstone concept lands on date 5),
then ~10 min reviews as cards come due (1/2/7/21-day boxes, ~31 days), plus one
~15-minute delayed assessment (estimate). You can stop after any step; progress is saved.

Needs: clang++ (C++23), Boost headers at /opt/homebrew/include, python3.

```bash
# start (or resume) — one lesson per sitting; use today's date
bash labs/build.sh
python3 drive.py lessons --progress progress.json --today 2026-11-14 --new-limit 4

# delayed assessment: once all 4 lessons are taught, wait >= 7 days, then (once)
python3 drive.py lessons --progress progress.json --today 2026-11-30 --assessment

# reset everything (archives old progress first)
python3 drive.py lessons --progress progress.json --today 2026-11-14 --reset
```
- `--new-limit 4` = 4 new concepts per date (the driver's own default is 6; use 4 here).
- Each lesson's pretests are asked on the date that lesson is taught, before its teaching.
- **Confidence is asked by default** before every key (`sure` / `unsure` / `0.8` / blank).
  `--no-ask-confidence` turns it off.
- A miss you marked `sure` (or >= 0.7) prints `!! HIGH-CONFIDENCE MISS`: it is re-tested
  first in the same session's retry pass and before every other due card next session,
  until you hit it on a later date.

No model? Read `STUDY.md` (keys at the end; mark confidence before each key) and run
`checks/*.check.sh` by hand (`echo 'answer' | bash checks/l1-bytes.check.sh; echo $?`).
The delayed assessment on paper is `assessment/FORM.md` (keys at the end).
Numbers come from an Apple M1 Max; re-run `bash bench/run_all.sh` on your own box.
**Warning:** `bash bench/run_all.sh` overwrites `bench/results/` (run1..run5 and median), so the numbers quoted in the lessons will no longer match those files. Copy the directory first if you want to keep it. Numbers moved 20–50% between two sessions on the same machine, so trust ratios and shapes.

Driver self-check (no model): `bash evals/replay-all.sh` replays a recorded 7-date run plus the
assessment into fresh progress and fails if any concept or card is left untouched.
