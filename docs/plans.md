# Plans

Work we've decided to do but haven't done yet, so it survives `/clear` and new sessions. Each
row points at where the detail lives; it doesn't restate it. TUI work goes in `tui_todo.md`,
bugs in `known_bugs.md`, and missing features with no decision yet in `not_implemented.md`.

Add a row whenever something is decided and deferred. When it lands, delete the row; git keeps
the history.

| Item | Decided | Spec | Next step | Who |
|---|---|---|---|---|
| Illegal opcodes trap through the kernel trap entry | 2026-11-08 | `interrupts_proposal.md` §8 | Settle where `TRAP_VECTOR` lives (§8 open question 1), then implement the minimal first cut | Owner, or a new session |
| Harden the parser: use the `expect()` results to report errors | 2026-11-08 | None yet | Write a spec. This also clears the 18 `-Wunused-variable` warnings in `parser.cpp`, which are deliberately kept until then | Owner |
| Warning triage, items still to decide | 2026-11-08 | PR #23 commits; `.clang-tidy` comments | Go through one by one: the 6 `bugprone-signed-bitwise` findings on signed variables, narrowing conversions, integer sign comparisons, implicit widening, the other unused variables and `TIMING`, cognitive complexity, swappable parameters, enum size, branch clones, math parentheses, pass-by-value, then the small mechanical checks. Run `tools/tidy-summary.py --update` at the end to set a baseline | Owner and Claude |
