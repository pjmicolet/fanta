---
name: fanta-task
description: File a new Project Fanta task from one line, after checking for duplicates. Use for "/fanta-task <one line>", "add a task", "make an issue for…". Runs inline; no agent.
---

# Fanta Task

Input: one line describing the task. Files it in about 3 tool calls.

1. **Dedupe:** run `tools/triage.py | grep -iE '<2–3 keywords>'` (not the whole digest). If a hit
   looks related, open it with `tools/triage.py --issue N` and offer the user three choices:
   **append** (comment on #N), **sub-issue** of #N, or **new** anyway.
2. **Draft:** title in the repo's style (`TUI: …`, a verb phrase for compiler/vm work). Write a
   body of 2–5 lines covering what's wrong or wanted and a rough approach; link any friction
   entry or issue it came from. Labels: one area (`tui`/`compiler`/`vm`) plus any of
   `bug`/`idea`/`friction`/`blocked`/`needs-decision`. Add the current milestone only if the user
   says it belongs in this release.
3. **Confirm**, showing title, labels and body in one block, then
   `gh issue create … ` and `gh project item-add 1 --owner pjmicolet --url <url>`. If it should
   come before something, `tools/triage.py --move N --after M`.
4. If it came from a friction entry, end that line in `docs/tui_friction.md` with `→ #N`.

Offline (`gh` fails): append `YYYY-MM-DD  <who>  <line>` to `docs/inbox.md` instead.
Reply with the issue URL only.
