# TUI Friction Log

This is where TUI changes come from. Anyone who uses the TUI (the owner `pj`, or Claude `cc`) adds a line whenever something gets in the way. Write down **what hurt, not the fix**. `/fanta-tui-review` reads this log, and a change to the TUI should point to the entry it resolves (see the fit test in `docs/tui_spec.md` §0).

Format: `date  who  what got in the way  [status]`. Status is blank while an entry is open, `→ spec §x` once a planned change covers it, `fixed <commit>` when done, and `wontfix: reason` if it's declined. Keep the entries; don't delete them. The history shows which parts of the design keep causing pain.

## Open

Seeded from the 2026-10-31 usability review (findings F1–F12, summarised in `docs/tui_spec.md` §9). Entries move to *Resolved* as the rewrite lands, milestone by milestone.

```
2026-10-31  cc  stepping a .fn program, nothing shows which source line is executing                                → spec §7 (needs line table)
2026-10-31  cc  edited the .fn source, kept stepping, and the old program ran with no sign it was out of date        → spec §2.1
2026-10-31  cc  failed compile keeps the old program loaded, but nothing says so once the message is gone           → spec §2.1
2026-10-31  cc  inserted a blank line in a running .asm and every address shifted under it (ISR no longer at 0x100) → spec §2.1, §5
2026-10-31  cc  a typo in the source froze the UI for 2s, then reported a parser hang with no line number           → spec §5 (diagnostics)
2026-10-31  cc  `return qd;` with qd undefined compiled without complaint                                          (compiler bug, not TUI)
2026-10-31  cc  memory pane is fixed at 0x0000 and 32 bytes, so it never shows what I'm looking at   [partly tui/layout: Memory tab fills the panel, [ ] scrolls; still doesn't follow anything]
2026-10-31  cc  :w with no filename gives "Unknown command", and q quits without asking even with unsaved edits     → spec §2
2026-10-31  cc  "Loaded ..." message stays up long after it means anything                                        → spec §6.2
2026-11-01  cc  can't start a new Fanta file from the TUI: ":e new.fn" on a missing file fails with "Error loading"
```

## Parked ideas

Ideas that don't come from friction. They wait here until an entry in the log gives them a reason.

```
2026-10-31  cc  breakpoints (TUI address set vs BRK instruction; undecided, spec §6.3)
2026-10-31  cc  run to cursor
2026-10-31  cc  editing registers and memory from the TUI
2026-10-31  cc  step back / reverse execution
```

## Resolved

```
2026-10-31  cc  the key hint lists 5 of about 15 keys; :help is an unknown command   [fixed tui/keys: per-mode hint bar, ? and :help overlay]
2026-10-31  cc  `c` runs but `:c` compiles; `r` silently recompiles a .fn   [fixed tui/keys: F-keys execute, s/c/r/o do nothing, F6 never rebuilds]
2026-11-01  pj  didn't know how to switch from editing assembly to writing Fanta code; nothing on screen says ":e x.fn"   [fixed tui/keys: help overlay explains :e x.fn / :e x.asm / :e x.bin]
2026-11-01  pj  Ctrl+C doesn't leave INSERT the way it does in vim   [fixed tui/layout: Ctrl+C is ESC everywhere]
2026-10-31  cc  at 80x24 the status block overwrites R8-R12, and the editor's first line runs into the memory hex   [fixed tui/layout]
2026-10-31  cc  stack values run into the disassembly and VRAM headers at 80x24   [fixed tui/layout]
2026-10-31  cc  editor doesn't follow PC in ASM mode: plasma runs at line 135 while the editor shows lines 0-13   [fixed tui/layout]
2026-10-31  cc  gutter shows decimal line numbers, so I had to work out addresses by hand (line*4)   [fixed tui/layout (address gutter; comment lines still take an address until the assembler listing, spec §5)]
2026-10-31  cc  after a program halts, PC sits on the instruction after HALT, which looks like main is starting again   [fixed tui/layout]
2026-10-31  cc  only the first changed register gets highlighted   [fixed tui/layout]
2026-10-31  cc  in COMMAND mode the cursor stays in the editor, not on the ':' line   [fixed tui/layout]
2026-10-31  cc  at 200x60 most of the screen is empty: editor gets 6 rows, memory and stack 4 each   [fixed tui/layout]
2026-10-31  cc  disassembly's top half is blank when PC is near 0   [fixed tui/layout]
2026-10-31  cc  long editor lines are painted over by the VRAM pane; serial pane lets editor text show through   [fixed tui/layout]
2026-10-31  cc  starfield: 1 star of 8 visible in the VRAM monitor; an all-black frame looks like an empty pane   [fixed tui/layout (averaging; half-blocks still to do, spec §6.4)]
2026-10-31  cc  serial and VRAM can't be seen at the same time   [fixed tui/layout]
2026-10-31  cc  a loaded .bin shows the same disassembly twice, once with line numbers and once with addresses   [fixed tui/layout]
2026-10-31  cc  PC shows 4 hex digits, SP 6 and the SP register 8   [fixed tui/layout]
2026-10-31  cc  IPS reads 0 in every headless screenshot   [wontfix tui/layout: IPS dropped from the status line; re-add if it's missed]
```
