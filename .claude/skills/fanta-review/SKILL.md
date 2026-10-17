---
name: fanta-review
description: Run Project Fanta's three-reviewer pass (performance, design, testing) in parallel and synthesise the results into a dated report under docs/reviews/. Use when the user asks for a review pass, "/fanta-review", or a review of the compiler/VM. Args - "full [paths...]" for a whole-tree pass, otherwise reviews the diff since main (or a given git range).
---

# Fanta Review (coordinator)

You — the main session — are the coordinator. You keep the user's context, so you synthesise;
the reviewers start cold.

## 1. Scope
- `full [paths...]`: review the given paths (default `compiler/ vm/`; `common/` headers they
  include are in scope for reading).
- No arg / a git range: review `git diff main...HEAD` plus uncommitted changes (default) or the
  given range. Pass both the file list and the range to reviewers.
- Always excluded: `build*/`, `scratch/`, `compile_commands.json`, untracked root files.
- Read the most recent report in `docs/reviews/` (if any) so you can mark findings as new,
  repeated, fixed or previously declined.

## 2. Shared baseline (once, before fan-out)
Run `tools/run-clang-tidy.sh` restricted to the scope (file regex) and save the output to
`<scratchpad>/review/clang-tidy.txt`. Create `<scratchpad>/review/{perf,design,test}/`.

## 3. Fan out — one message, three Agent calls in parallel
`fanta-perf-reviewer`, `fanta-design-reviewer`, `fanta-test-reviewer`. Each prompt includes:
the scope (paths or range), the absolute path of its own scratch dir, the clang-tidy output path,
and the previous report path if any. Do not restate the agent's instructions.

## 4. Synthesise
- **Verify before reporting.** Spot-check each high-severity finding against the code; drop or
  downgrade anything that doesn't hold. Never invent results for a reviewer still running.
- **Deduplicate** findings that hit the same location/issue.
- **Conflicts:** where a perf finding and a design finding touch the same code or pull in opposite
  directions, put them side by side in a Conflicts section with both sides' cost — give a
  recommendation, but leave the decision to the user.
- **Tests:** add the tester's `passes` tests to the existing `test/*.cpp` files (allowed domain),
  build and run `ctest` in `build/`, and report results. `fails` tests are probable bugs: do not
  add them to the suite and do not touch `compiler/`/`vm/`/`common/` — list them under Probable
  bugs with the test code so the user can reproduce. Tests that need a new file / CMake change are
  proposed, not applied.
- Perf/design examples stay as examples in the report — never applied to the code.

## 5. Report
Write `docs/reviews/YYYY-MM-DD-<scope>.md`:
1. Summary (5 lines max) and top 3 actions.
2. Probable bugs (from failing tests or verified findings).
3. Conflicts (perf vs design).
4. Performance findings (P#), design findings (D#) — keep the reviewers' format and examples.
5. Tests added (file, names, ctest result) and tests proposed but not applied.
6. Status of findings from the previous report.

Then reply to the user with the summary, the bug list, the conflicts and the report path — not the
whole report.
