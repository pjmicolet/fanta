---
name: fanta-triage
description: Backlog triage for Project Fanta. Spawns the fanta-orchestrator agent to rank, merge and push down GitHub issues, then applies the changes the user approves. Use for "/fanta-triage", "what's pending", "what next", "prioritise" or "clean up the backlog". Args - optional focus (e.g. "tui", "v0.0.2", "merge only").
---

# Fanta Triage (coordinator)

The agent reads the backlog so the `gh` output stays out of this session. You present its plan and
apply what the user approves.

## 1. Spawn
Use the Agent tool with `subagent_type: fanta-orchestrator`. Pass only the user's focus, if any,
plus anything from this conversation that changes priorities (for example, "user is about to start
#26"). Don't restate the agent's instructions.

## 2. Present
Show the agent's plan as it is. Don't re-run the digest to check it, but spot-check any `close`
item with `tools/triage.py --issue N` before you offer it. Ask which numbered changes to apply
(for example, "all", "1,3,5" or "none").

## 3. Apply the approved changes only
| Change | Command |
|---|---|
| move | `tools/triage.py --move N --after M` / `--move N --top` |
| merge A into B | `gh issue comment B -b "<moved text> (from #A)"`, then `gh issue close A -r "not planned" -c "Merged into #B"` |
| sub-issue | `gh api graphql -f query='mutation{addSubIssue(input:{issueId:"<B node id>",subIssueId:"<A node id>"}){clientMutationId}}'` (node ids from `gh issue view N --json id`) |
| close | `gh issue close N -c "<reason>"` |
| label / milestone | `gh issue edit N --add-label x --remove-label y` / `--milestone v0.0.2` |
| new issue | `gh issue create -t … -b … -l … [-m …]`, then `gh project item-add 1 --owner pjmicolet --url <url>` |
| friction link | edit the entry in `docs/tui_friction.md` to end with `→ #N` |

For any inbox line that became an issue, delete it from `docs/inbox.md`. Report what was applied
in one line per change. Don't commit unless the user asks.
