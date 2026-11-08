# TUI TODO

The work queue for the TUI, in order. Each item points to the `tui_friction.md` entry it resolves
(spec §0 fit test, question 1); the friction log keeps the full wording and history. When an item
lands, mark its friction entry `fixed` and delete the item here. Git keeps the history.

## Ready (TUI-only)

1. **Headless `--size` for F10 and `:out`.** `term_w`/`term_h` are only set in `update()`, so
   screenshots of the output panel are taken at 80x24. Do this early: every later item is checked
   with `--screen`. Friction 2026-10-31 "headless screenshots".
2. **80x24 batch:** status line drops messages and cuts PC, flags cut off, pane titles truncated
   without `~`, help overlay loses its last 3 lines. One layout pass. Four 2026-10-31 entries.
3. **Status line noise batch:** "Loaded ..." never expires (spec §6.2), STALE shown three times,
   hint bar says "F5 run" while RUNNING, "Line 25 added" is 0-based. Four 2026-10-31 entries.
4. **Build pane honesty:** after a failed build the pane says build #2 but shows build #1; a new
   `.fn` shows build #0 full of HALTs. Two 2026-10-31 entries.
5. **BINARY session:** j/k do nothing and the hint bar offers `i edit` and `F9 build`.
6. **Memory tab follows something** (SP, or the last address written). Needs a decision on what
   it follows: ask the owner first.
7. **Serial timing in the Serial tab** (once PR #18 is merged): title shows `TX busy` while a
   byte is going out and `N dropped` when `Serial::dropped > 0`, so a program that skips the
   STATUS poll fails visibly. Log the friction first: run a program that doesn't poll.
8. **Cycle count instead of IPS:** show `CPU::cycles()` (and the nominal clock from
   `CPU::spec`) in the registers pane or status line. Replaces the IPS figure that was dropped.
   Needs a friction entry, e.g. "can't tell how long a @print waited".
9. **Fix `test/serial_hello.asm`:** since PR #18 it prints only `H`, because it doesn't poll
   STATUS, and it still mentions `o`. It's untracked, so tests can't rely on it.
10. **Check and close if already fixed:** VRAM program opens on the Serial tab (owner decided the
   panel follows the last device written); STALE gutter addresses (owner decided: dim the gutter);
   the help overlay's `o` mention.

## Blocked on user-owned code

| Item | Waits on | Friction |
|---|---|---|
| Decode UTF-8 in the Serial tab, showing non-ASCII as `?` or `<U+00E9>` | owner decision: `@print` takes a code point and sends it as UTF-8 bytes (discussed 2026-11-07, not decided) | 2026-11-07 `@print(300)` |
| `>` marker in SOURCE, source-line stepping, `main+0x2C` symbols | compiler line table (spec §7) | 2026-10-31 source line |
| Gutter marker on the failing source line | compile diagnostics with line numbers; tokens carry line 0 (`not_implemented.md`) | 2026-10-31 parser hang |
| `:eject` on compiled code | B16 in `vm/string_assembler.hpp` | |
| Real address gutter, comments cost no address | assembler listing (spec §5) | |
| Build of `let b : int = ;` crashes the TUI | `emitVariableIR` out-of-bounds read | 2026-11-07 |

## Needs an owner decision

- **Half-block VRAM** (spec §6.4): links ncursesw, and `--screen` must read wide cells back.
- **Tab focus:** deferred until a second pane scrolls.
- **Run to cursor / run until the loop exits:** at 200 MHz a STATUS poll loop spins about
  5,800 times per character, so F7 through one is unusable. Parked until someone writes a
  polling loop and logs it.
- **Parked ideas** in `tui_friction.md` (breakpoints, run to cursor, edit registers, step back):
  wait for friction.
