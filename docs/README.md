# Fanta Documentation

Read only what the task needs.

| Document | Covers | Status |
|---|---|---|
| `developer_guide.md` | Architecture, memory map, stack frames, encoding, compiler pipeline | Describes today |
| `parser.md` | Lexer, parser, implemented grammar | Describes today |
| `pratt_parser.md` | Step-by-step traces of expression parsing | Tutorial |
| `comparisons.md` | How comparisons lower to `CMP` and branches | Describes today |
| `commenting_standards.md` | Doxygen conventions | Standard |
| `review_checklist.md` | Project rules to check in code reviews | Standard |
| `language_design.md` | Planned language, `comptime`, first kernel goal | Draft design |
| `adaptive_cpu.md` | Self-reconfiguring CPU unit and its contracts | Draft design |
| `cooperative_interrupts.md` | `CIP`, the user event layer | Agreed spec, not yet implemented |
| `interrupts_proposal.md` | Traps and the kernel layer, including the mode bit | Proposal |
| `low_latency_architecture.md` | Why events are cooperative; where older ideas went | Rationale |
| `tui_spec.md` | TUI design | Agreed spec |
| `tui_friction.md` | TUI problems log | Living log |
| `tui_todo.md` | TUI work queue, pointing at friction entries | Living log |
| `known_bugs.md` | Reproduced bugs with causes, to check before debugging | Living log |
| `not_implemented.md` | Missing features, dated so they can be re-validated | Living log |
| `reviews/` | Dated review reports | Historical, not updated |

When documents overlap, the code wins for current behaviour, and the specs win for planned behaviour.
