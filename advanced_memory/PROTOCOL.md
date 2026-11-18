# Tutor protocol — Pooled, hierarchical memory for hot-path containers (T2/T3)

Use the published `MANIFEST.md`, `lessons/*.md`, `STUDY.md`, and progress state. The model selects and explains authored material; it never invents a curricular question, hint, answer, stretch, or grading criterion. Every authored field is available to paper and script learners.

## Session contract

- State the estimated time cost and the chosen limits before starting. Default to six **new concepts**, not six topics (T1: `--new-limit 6`, the default). Show nothing from a lesson whose concepts are not taught that day: its leading prose and pretests wait for the day its first concept is taught. Let the learner stop after any step and resume from `_runtime.completed_steps` or a pending exposed answer.
- Resolve `Concepts:` within a lesson; use `lesson-02.md#L2C1` for another lesson (concept labels are globally unique: `L<lesson>C<n>`). `[topic:]` only groups potentially confusable cards for interleaving. A synthesis card waits for every referenced concept. If a legacy card has ambiguous source, teach its whole lesson before asking it.
- For each new concept: ask its authored pretest before teaching, show the complete concept and text equivalent of any diagram, collect the authored elaboration and hands-on response, then ask its completion, recall and application cards. Show the relevant keys after each attempt. Resume unfinished work before introducing more material.
- Ask one answer at a time. Never reveal a core key before its attempt. `:hint` may show only the authored hint. After core feedback, offer the authored stretch for a separate optional attempt (blank skips), then reveal `StretchKey:`. The stretch never replaces the core attempt or changes its box/date. Grade against the full published key; explain the verdict in at most four sentences. A conceptual explanation needs the entire checklist, not a keyword match.
- Offer the full authored material and keys for browsing at any time. Browsing a key before an attempt is assisted study, not unaided retrieval.
- Preserve four boxes and intervals 1/2/7/21 days. A first hit enters box 1, due tomorrow. A later-date hit can promote once that day. A miss stays due tomorrow even if a same-day retry hits. Limit attempts to three per card/date. Retry a miss only after unrelated work; otherwise defer it. Suppress a scaffold only when an explicitly linked independent check succeeds.
- Count every answer prompt in the interaction budget. Check time between steps; an in-progress answer may overrun the estimate. End with the next due date and the exact pending step, if any.

## Grading and parity

For v2, commit each completed practice attempt in `_runtime.attempt_records` with
the card and content version, objective, tier, grade, grader source, assistance,
answer and feedback times, and feedback exposure. Commit it in the same progress
replacement as the box update. Ask response confidence after every answer and
before any feedback (T1 does this by default; `--no-ask-confidence` opts out; the
old `--ask-confidence` flag is a no-op). Blank is null; numeric 0–1 or percent is
recorded as supplied; `sure`/`unsure` is qualitative only. Do not infer a missing
probability. A miss marked `sure` or >= 0.7 is a high-confidence miss: say so
(T1 prints `!! HIGH-CONFIDENCE MISS`), mark it `hypercorrection`, re-test it first
in the same-session retry pass and before every other due card next session, and
clear the flag only after a hit on a later calendar day. List flagged cards at the
end of the session. Tagged replay may include an optional `confidence<TAB>value`
after an `answer` line; a missing line means blank confidence.

`assessment/form.json` (28 items: one retention and one transfer item per objective,
none of them a lesson card) is given once, at least 7 days after lesson 07 is finished;
its prerequisites are the 14 concepts it names, so all must be taught. Use its fixed items after declared teaching
prerequisites. Ask every item and save each response before showing any key.
Grade against the form's complete bullets, record actual elapsed time since the
last relevant exposure, and label a reused form exposed. Assessment items stay
outside routine study and box scheduling. T0 uses `assessment/FORM.md`; T1 uses
`drive.py --assessment`; T2/T3 read the same form and keys. An assessment can
resume after any saved response or grade.

`Key:` and `StretchKey:` are complete criteria. HIT covers all bullets; PARTIAL covers some; MISS covers none, contradicts one, or is skipped. The model may accept a correct paraphrase, but must not change a criterion. Show every bullet after an attempt, including a hit. Source and figure text stay visible. Paper users follow `STUDY.md` with keys after the study section; T1 users can run `drive.py --browse` or `--print-t0` without a model or network.

Before use, run the v2 structural audit (`python3 evals/audit.py .`). A passing
audit checks structure; it does not verify claims or make a transfer task
substantively varied. Preserve the same authored tasks and complete keys for
every tier.

Tagged replay is a test interface (`--replay`; `--record FILE` writes one), with one `tag<TAB>value` per prompt: `answer`, `confidence`, `grade`, `stretch`, `continue`, `elaborate`, or `hands_on` (assessment: `assessment_answer`, `confidence`, `assessment_grade`). `bash evals/replay-all.sh` replays the recorded full course into fresh progress. A blank stretch value skips it. Unversioned tutorials retain their old `--auto` contract and now show authored lesson content before new practice without consuming a replay line.

## Redoing the tutorial

Run `drive.py lessons --progress progress.json --today YYYY-MM-DD --reset` to
redo everything, or `--reset-lesson lesson-01.md` to redo one lesson from its
pretest. Each command archives the old progress beside it as
`progress.YYYY-MM-DD.bak.json` (adding `.1`, `.2`, etc. before `.json` if needed)
and exits without starting a session. A missing progress file changes nothing;
an unknown lesson lists valid names. A lesson reset retains all past attempt
records and the other lessons' state; a full reset's attempts remain in its
archive. A full reset archives even corrupt progress; a lesson reset requires
readable progress and suggests `--reset` when it is corrupt. Inspect an archive directly with `evals/outcomes.py` for earlier
outcomes; the current progress report covers its own records only.

Paper equivalent: copy your dated tracking sheet into an archive first. For a
full redo, begin a blank sheet. For one lesson, erase only that lesson's checked
steps, pretest marks, boxes, due dates and unfinished answer; leave other lessons
and the old attempt log intact. Start the selected lesson at its pretest and
continue through its teaching, prompts and cards. Keep the archived sheet for
the earlier attempt history.

## Tutorial-specific notes

- Code cards (`AcceptIf: check(checks/...)`) compile the learner's answer with clang++ against
  `labs/hft_mem` and Boost headers (override with `CXX`, `BOOST_INC`). Run the check; never judge
  code by reading it. If the toolchain is missing, fall back to self-grading against the full key
  and say so.
- Before lesson 01, have the learner run `bash labs/build.sh`. Every HandsOn step names the lab
  binary to run and its expected output is the `HandsOnKey:`.
- Numbers in the lessons were measured on an Apple M1 Max (see MANIFEST.md). Do not present them
  as the learner's numbers; ask the learner to run `bash bench/run.sh` on their own box.
- Linux-only content (huge pages, NUMA) is labelled NOT executed on the authoring machine. Keep
  the label when you explain it.
