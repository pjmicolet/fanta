# TUI Friction Log

This is where TUI changes come from. Anyone who uses the TUI (the owner `pj`, or Claude `cc`) adds a line whenever something gets in the way. Write down **what hurt, not the fix**. `/fanta-tui-review` reads this log, and a change to the TUI should point to the entry it resolves (see the fit test in `docs/tui_spec.md` §0).

Format: `date  who  what got in the way  [status]`. Status is blank while an entry is open, `→ spec §x` once a planned change covers it, `fixed <commit>` when done, and `wontfix: reason` if it's declined. Keep the entries; don't delete them. The history shows which parts of the design keep causing pain.

## Open

Seeded from the 2026-10-31 usability review (findings F1–F12, summarised in `docs/tui_spec.md` §9). Entries move to *Resolved* as the rewrite lands, milestone by milestone.

```
2026-10-31  cc  stepping a .fn program, nothing shows which source line is executing                                → spec §7 (needs line table)
2026-10-31  cc  a typo in the source froze the UI for 2s, then reported a parser hang with no line number           → spec §5 (diagnostics)
2026-10-31  cc  `return qd;` with qd undefined compiled without complaint                                          (compiler bug, not TUI)
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
2026-10-31  cc  memory pane is fixed at 0x0000 and 32 bytes, so it never shows what I'm looking at   [fixed tui/review1-fixes: :mem ADDR jumps the Memory tab (replaces [ ])]
2026-10-31  cc  "Loaded ..." message stays up long after it means anything   [fixed tui/review1-fixes: messages expire on the next key; build errors stay; all kept in the Build log]
2026-10-31  cc  after inserting a line in a running .asm (STALE), the gutter and > show the edited text's addresses, not memory's: >0x000064 sits on a blank line while memory at 0x64 is still JMP MAIN_LOOP   [fixed tui/review1-fixes: gutter dimmed, > hidden until F9 (Q3)]
2026-10-31  cc  "Line 25 added/removed" is a 0-based line number, but the ASM gutter only shows addresses, so I couldn't find line 25   [fixed tui/review1-fixes: the message names the address]
2026-10-31  cc  at 80x24 the status line drops messages entirely and cuts the PC; a compile error is only visible via F10 > Build log   [fixed tui/review1-fixes: short status forms keep room for the message]
2026-10-31  cc  at 80x24 the flags are cut off in every session ("R11 00000000 F~")   [fixed tui/review1-fixes: compact registers end with F znvc]
2026-10-31  cc  at 80x24 the pane titles lose "(source of truth)" / "(derived ...)" with no "~": "(source -", ":ej-"   [fixed tui/review1-fixes: short titles when narrow]
2026-10-31  cc  after a failed build the derived pane is titled "build #2" but shows build #1's code   [fixed tui/review1-fixes: the title names the build in memory]
2026-10-31  cc  pressing i, : or z pauses a running program; "edit while it runs" means edit while paused, then F5 again   [wontfix (Q1): editing pauses on purpose; z no longer pauses]
2026-10-31  cc  a VRAM program at 120x40 opens on the Serial tab showing "(no serial output)"; I had to F10 to see the ball   [fixed tui/review1-fixes: the panel follows the device last written (Q5)]
2026-10-31  cc  editing a .fn shows STALE three times (badge, pane title, "Edited: STALE until F9" message)   [fixed tui/review1-fixes: the Edited message is gone]
2026-10-31  cc  in a BINARY session j/k do nothing visible and there's no way to look at code away from PC; the hint bar still offers "i edit" and "F9 build"   [fixed tui/review1-fixes: no hidden buffer; hint drops i and F9; F9 does nothing]
2026-10-31  cc  the hint bar says "F5 run" while the program is RUNNING   [fixed tui/review1-fixes]
2026-10-31  cc  at 80x24 the help overlay silently loses its last 3 lines (q quit, ESC closes the overlay, the macOS fn note)   [fixed tui/review1-fixes: help fits in 20 lines]
2026-10-31  cc  a new .fn shows "ASM build #0 (derived ...)" full of HALTs from empty memory   [fixed tui/review1-fixes: COMPILED (not built yet)]
2026-10-31  cc  headless screenshots: F10 and :out act as if the terminal were 80x24 whatever --size says (term_w/term_h are only set in update())   [fixed tui/review1-fixes]
2026-10-31  cc  test/serial_hello.asm still says "Press 'o' in the TUI to see it"; o does nothing now   [fixed in the working copy only: test/*.asm is gitignored (.gitignore: *.asm), so the demos aren't in the repo; owner to decide]
2026-10-31  cc  edited the .fn source, kept stepping, and the old program ran with no sign it was out of date   [fixed tui/session: [+] and STALE in the status line, derived ASM dimmed]
2026-10-31  cc  failed compile keeps the old program loaded, but nothing says so once the message is gone   [fixed tui/session: STALE stays until a build succeeds]
2026-10-31  cc  inserted a blank line in a running .asm and every address shifted under it (ISR no longer at 0x100)   [fixed tui/session: edits that move code aren't patched; STALE until F9]
2026-10-31  cc  :w with no filename gives "Unknown command", and q quits without asking even with unsaved edits   [fixed tui/session: :w uses the session's file; q/:q refuse with unsaved changes, :q! forces]
2026-11-01  cc  can't start a new Fanta file from the TUI: ":e new.fn" on a missing file fails with "Error loading"   [fixed tui/session: :e on a missing .fn/.asm opens an empty buffer, :w creates it]
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
