---
name: fanta-tui-review
description: Run Project Fanta's periodic TUI design review. A usability reviewer drives the real TUI headlessly, checks it against docs/tui_spec.md, works through docs/tui_friction.md, and looks for things to remove; the coordinator verifies, updates the friction log and spec, and writes docs/reviews/YYYY-MM-DD-tui.md. Use at milestones (a batch of TUI changes landed, or a spec §5 interface landed in vm/compiler), or when the user asks for "/fanta-tui-review" or a TUI usability/design review.
---

# Fanta TUI Review (coordinator)

You are the main session and the coordinator. You hold the user's context, so you verify and
decide; the reviewer starts cold. The purpose is to make the TUI **evolve from use**, not to keep
adding features. See `docs/tui_spec.md` §0.

## 1. Is a review due?

Check `docs/tui_friction.md` and the last `docs/reviews/*-tui.md`.

- If there's no new friction since the last review, no new TUI commits (`git log -- tui/`), and no
  new §5 interface (`git log -- vm/ compiler/ common/`), say so and stop. Don't run a review that
  has nothing to work from.
- Otherwise, note the git range since the last review. If there is no previous review, use the
  whole history.

## 2. Prepare

- Build: `cmake --build build -j8 --target fanta-tui fanta-compiler tui_test`. Then run `ctest` in
  `build/` and note the result.
- Create `<scratchpad>/tui-review/`.

## 3. Spawn one reviewer

Use the Agent tool with `subagent_type: fanta-tui-reviewer`. The prompt contains:

- the scratch directory's absolute path;
- the git range since the last review;
- the path of the previous TUI review, if any;
- anything the user asked to focus on.

Don't restate the agent's own instructions.

## 4. Verify and decide

- **Verify before reporting.** Open the screenshots the reviewer names and check the quoted lines.
  Re-run a screenshot yourself for any high-impact claim. Drop anything that doesn't reproduce.
- **Friction log** (`docs/` is an allowed domain):
  - Move resolved entries to *Resolved* with `fixed <commit>`.
  - Paste the verified new entries into *Open*.
  - Never delete entries.
- **Spec drift:** if the code is right and the spec is behind, update `docs/tui_spec.md` and say
  so. If the spec is right, the drift becomes a friction entry or a task.
- **Removal candidates and design questions** go to the user. Don't decide them yourself.
- **Fit test:** for any recent change that fails spec §0, name it and the question it doesn't
  answer.

## 5. Report

Write `docs/reviews/YYYY-MM-DD-tui.md`:

1. Summary (at most 5 lines) and the top 3 actions.
2. Friction status: resolved / still open / new.
3. Spec drift and what was done about each item.
4. Fit with the system: interfaces that landed, and workarounds.
5. Removal candidates.
6. Design questions for the owner.
7. The screenshots worth keeping. Copy them into `docs/reviews/tui-screens/YYYY-MM-DD/` only if
   the report refers to them.

Then reply to the user with the summary, the questions and removal candidates, and the report
path. Don't paste the whole report.
