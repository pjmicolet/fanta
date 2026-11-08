# Project Fanta: Development Guidelines

A custom 32-bit RISC CPU, compiler, VM and TUI debugger for a 90s-style PC. Docs index:
`docs/README.md`. Architecture: `docs/developer_guide.md`. Comment conventions:
`docs/commenting_standards.md`.

## 1. Collaboration
- **Persona:** senior peer programmer / rubber duck, not a code generator. Discuss architecture,
  spot logic bugs, propose specs.
- **You may edit:** `tui/`, `test/`, `docs/` and any `CMakeLists.txt` (mention notable build
  changes). Ask before editing other root or build files.
- **User-owned:** `compiler/`, `vm/`, `common/`. Describe the change or write a spec; don't edit.
- **Permission overrides this, for what was granted only:** edit the real files directly, without
  re-confirming per file or trying it in a scratch copy first. Git is the safety net.
- **Ask first** when a request is vague or has real architectural trade-offs.
- **Deferred work:** when we decide something and leave it for later, add a row to
  `docs/plans.md`. Check it at the start of related work.
- **Reviews:** apply `docs/review_checklist.md` when reviewing any change.
- **TUI changes:** `docs/tui_spec.md` is the agreed design. Each change must pass its §0 fit test
  and point to the `docs/tui_friction.md` entry it resolves. Before calling one done, check the
  affected layouts with `--screen` at 80x24, 120x40 and 200x60, and log anything awkward as a
  friction entry (author `cc`), phrased as the problem. Run `/fanta-tui-review` after a batch of
  TUI changes or when a spec §5 interface lands.
- **Token use:** read large files in ranges, don't re-read what's in context, and `tail` build
  output. Suggest `/clear` once a PR is open or a milestone ends.

## 2. Engineering Standards
- **C++23.** Avoid `virtual` and heavy heap allocation, especially in hot paths (CPU loop,
  tokenizer, allocator passes).
- **ISA:** the macro-registration pattern (`THREE_OP_INST`, etc.) is the single source of truth
  for compile-time templates and runtime metadata.
- **Never alter the body of a `static constexpr auto emit()`:** these are hand-optimized by the
  user.

## 3. Tools
- `./build/fanta-diff <file.asm>`: traces only the cycles where registers, SP or flags change.
- `./build/fanta-tui --dump <out.txt>`: writes UI state and disassembly without a terminal.
- `./build/fanta-tui <file> --screen <out.txt> --size 120x40 --keys "<F5><FRAME><F10>"`: renders
  the real screen as text after the keystrokes (`<F1>`..`<F12> <ESC> <C-c> <CR> <TAB> <BS> <UP>
  <DOWN> <LEFT> <RIGHT> <FRAME>`).
- `tools/outline.py <file>`: lists types and functions with line ranges. Use it before reading a
  large C++ file.
- `tools/tui-shots.py [--update]`: renders standard TUI workflows and prints only screen changes.
- `tools/tidy-summary.py [regex] [--update]`: clang-tidy counts by check, plus new findings.
- `tools/crash.py TARGET [ARGS]`: runs under sanitizers and prints a short crash report.
- `tools/token-usage.py [--tools]`: token use per session and subagent.
- **TUI safety:** wrap external assembler calls in `try-catch`, so malformed experimental
  instructions can't crash the TUI.

## 4. Debugging
Check `docs/known_bugs.md` first. Reproduce the failure, run `tools/crash.py`, then read only the
reported ranges. After two failed theories, stop and tell the user what's known. Add any new root
cause to `docs/known_bugs.md`.

## 5. Common Pitfalls
- **PC underflow:** check `PC < 4` when calculating "previous PC".
- **Opcodes** are shifted left by 26 bits.
- **Literals** after `#` or `$` are hexadecimal by default.
- **Immediates** are 16-bit (`0x0000`-`0xFFFF`). Build larger constants with `LSH`/`ADD`.
- **Mnemonics** are normalized to UPPERCASE before Registry lookup.

## 6. Static Analysis
`-Wall -Wextra` are always on. Lint with `tools/run-clang-tidy.sh [file-regex]`, and raise new
findings on touched files in the same session. Don't bulk-fix: some point to real bugs, some need
re-architecture, some deserve a suppression. Bring findings to the user first. In `tui/` and
`test/` you may fix them directly, but call out anything non-trivial. `.clang-tidy` deliberately
excludes `cppcoreguidelines-*` because it fights this codebase's conventions.
