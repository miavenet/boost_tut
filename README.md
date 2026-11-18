# Boost for low-latency C++: **start with boost_multi_index, then follow the order below**
- Order: **1 multi_index -> 2 flat containers -> 3 advanced_memory lessons 01-04 -> 4 lessons 05-07 (intrusive)**
- Each: recall-first, resumable, stop after any step. Same command to start and resume. Needs clang++ (C++23), Boost 1.90 in /opt/homebrew/include, python3.
## 1. boost_multi_index (`cd boost_multi_index`)
- Time (MANIFEST estimate): ~110 min, 4 sittings; ~10 min per review day
- Answers: when should a multi-key order store be one multi_index_container, and what is its tail?
- Start/resume: `bash labs/build.sh` then `python3 drive.py lessons --progress progress.json --today $(date +%F) --new-limit 5`
## 2. boost_flat_containers (`cd boost_flat_containers`)
- Time (MANIFEST estimate): ~105 min, 4 sittings of 25-30 min; ~10 min per review day
- Answers: which lookup table (flat_map, std, unordered_flat_map, direct index, Eytzinger) and how to kill insert spikes?
- Start/resume: `bash labs/build.sh` then `python3 drive.py lessons --progress progress.json --today $(date +%F) --new-limit 4`
## 3-4. advanced_memory (`cd advanced_memory`)
- Time (MANIFEST estimate): 01-04 ~110 min (4 sittings); 05-07 ~90 min more (3 x ~30); reviews ~5-8 min
- Answers: how do I give any hot-path container pooled memory with two lines, then open the box? (05-07: intrusive containers over a pool)
- Start/resume: `bash labs/build.sh` then `python3 drive.py lessons --progress progress.json --today $(date +%F)`
## Bridge
- **`bash bridge/build.sh`** runs the one-line allocator swap from tutorials 1 and 2 using the advanced_memory library. See `bridge/README.md`.
## Caveats
- **Numbers were measured on a shared Apple M1 Max: re-run on your box.**
- **NUMA and huge-page code is compiled but never executed on real NUMA hardware.**
- Times are author estimates, not measured with learners.
- No Python? Read each tutorial's `STUDY.md`.
- Reset: add `--reset` to the drive.py command (old progress is archived).
- Each answer asks how sure you were; a confident miss is retested first. Turn off with `--no-ask-confidence`.
- Delayed test: a week or more after the last lesson, add `--assessment` to the drive.py command.
