# Boost.MultiIndex for low-latency trading

4 lessons on one decision: when a trading data set (an order store by id, by
(side, price, time) and by client) should be a `boost::multi_index_container`, how
to build it correctly, and what its latency tail looks like.

Each lesson is recall-first: guess, run a lab, answer from memory. Code answers are
graded by checks that compile and run them.

**Time:** about 110 min first pass in 4 sittings (one lesson per study day), then
about 10 min per review day (estimate), plus an 8-min delayed assessment.
Needs clang++ (C++23), Boost 1.90 headers in /opt/homebrew/include (or set
`BOOST_INC`), and python3.

**Start** (from this directory):
```
bash labs/build.sh                                                  # build labs + benches
python3 drive.py lessons --progress progress.json --today $(date +%F) --new-limit 5
```
- **Always pass `--new-limit 5`** (= one lesson's 5 concepts per day; the driver's own default is 6).
- **Confidence is asked by default** before each key (`sure`, `unsure`, `0.8`, `80%` or blank). Add `--no-ask-confidence` to skip the prompt.
- **A miss you marked `sure` (or >= 0.7)** prints `HIGH-CONFIDENCE MISS`. It is re-asked first later in the same session and before every other due card next session, until you hit it on a later day.
- Other misses come back tomorrow. Hits are spaced 1 / 2 / 7 / 21 days, so the reviews run over about 31 days.

**Resume:** run the same command again. It picks up from `progress.json`.
**Days:** first pass 4 study days, then due reviews; the last ones land about 31 days in.
**Delayed assessment:** at least 7 days after you finish Lesson 04, run once:
`python3 drive.py lessons --progress progress.json --today $(date +%F) --assessment`
(12 unseen items, a retention and a transfer item per objective; it saves every answer before showing any key). Paper version: `assessment/FORM.md`.
**Reset:** `python3 drive.py lessons --progress progress.json --today $(date +%F) --reset`
(the old progress is archived next to it).

**No Python?** Read `STUDY.md` (the paper version, keys at the end; mark confidence before each key).
All numbers were measured on an Apple M1 Max. Re-run `bench/` on your own box.

**Proof it runs without a model:** `bash evals/replay-all.sh` replays a recorded 6-day run (every concept, every card, real checks, the assessment) into fresh progress.
