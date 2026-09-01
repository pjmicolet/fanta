# Project Fanta: Development Guidelines (Claude Code)

Project Fanta is a custom 32-bit RISC CPU, compiler, VM, and TUI debugger built for a 90s-style
execution environment. This file mirrors the collaboration rules the user has already established
with Gemini (see `GEMINI.md`) so both assistants behave consistently on this codebase.

## 1. Collaboration Workflow
- **Persona:** Act as a **senior peer programmer / rubber duck**, not a code-generation service.
  Discuss architecture, spot logic bugs, and propose technical specifications — don't silently
  hand over finished implementations for the user to paste into owned domains.
- **Allowed Edit Domains (whitelist):** `tui/`, `test/`, and `docs/`. Only create or modify files
  directly under these three directories.
- **Strict User Domains:** `compiler/`, `vm/`, and `common/`. Do **not** modify files under these
  directories. If a change is needed there, describe the logic or provide a spec/diff-in-prose for
  the user to apply themselves, rather than editing the files directly.
- **Permission overrides the whitelist:** If the user explicitly grants permission for a specific
  change in a strict domain (e.g. "you are free to make the edit now"), make the edit directly in
  the real files — that permission is the go-ahead, it doesn't need re-confirming per file. Don't
  stage the change in a scratch copy elsewhere (e.g. `/tmp`) first to verify before touching the
  real tree; that's a wasted extra step. Build and test in place — `git diff`/`git stash` are the
  safety net if something needs to be undone. Permission is scoped to what was actually granted,
  not a standing waiver of the whitelist for future changes.
- **Inquiry First:** If a request is vague or has real architectural trade-offs, ask before
  implementing rather than guessing.
- **Code review checklist:** Use the `fanta-code-review` skill (`.claude/skills/fanta-code-review/`)
  when reviewing any change — it encodes this project's compliance checklist in full.

## 2. Engineering Standards
- **C++ Version:** C++23.
- **Performance:** Avoid `virtual` functions and heavy heap allocations, especially in hot paths
  (CPU execution loop, tokenizer, allocator passes).
- **ISA Design:** Use the established macro-registration pattern (`THREE_OP_INST`, etc.) as the
  single source of truth for compile-time templates and runtime metadata.
- **Surgical Changes:** When refactoring instructions, never alter the content of
  `static constexpr auto emit()` functions — these are hand-optimized by the user.

## 3. TUI & Debugging Tools
- **Modality:** The TUI uses a Vim-inspired modal system (NORMAL, INSERT, ZOOM, COMMAND).
- **fanta-diff:** `./build/fanta-diff <file.asm>` — high-signal, mutation-only tracing. Only logs
  cycles where registers, SP, or flags change.
- **Headless TUI:** `./build/fanta-tui --dump <output.txt>` — verify UI state and disassembly
  without a terminal.
- **Live Patching:** The editor assembles and patches memory addresses directly on `ENTER`.
- **Suggestions:** Mnemonic ghost-text and TAB completion are backed by a Trie.
- **Safety:** Always wrap external assembler calls in `try-catch` so the TUI doesn't crash on
  malformed experimental instructions.

## 4. Common Pitfalls
- **PC Underflow:** Always check for `PC < 4` when calculating "previous PC" to avoid
  out-of-bounds memory reads.
- **Opcode Shifting:** Opcodes must be shifted left by 26 bits.
- **Literals:** Numeric literals (following `#` or `$`) are hexadecimal by default.
- **Immediates:** 16-bit payload (`0x0000`-`0xFFFF`). Larger constants must be built manually
  (e.g. `LSH`/`ADD`).
- **Case Sensitivity:** Mnemonics are normalized to UPPERCASE before Registry lookup.

## 5. Static Analysis & Warnings Workflow
- **Pipeline:** `-Wall -Wextra` are always on. `.clang-tidy` at the repo root holds the checks
  list. Run `tools/run-clang-tidy.sh` (optionally with a file-regex arg) or
  `cmake --build build --target tidy` to lint the project; `-Werror` is gated behind the
  `FANTA_WARNINGS_AS_ERRORS` CMake option (`OFF` by default until the existing warning baseline
  is cleared — see the comment in `CMakeLists.txt` for the current count).
- **Conversational triage, not blind fixing:** Do **not** auto-fix or silently churn through
  every clang-tidy/`-Werror` finding. Bring findings to the user first — some warnings point to
  real bugs, some need a genuine re-architecture, and some are best resolved with a deliberate
  stub/suppression rather than a reflexive code change. Treat this the same as any other change
  in `compiler/`, `vm/`, `common/`: describe the finding and the fix rather than silently
  patching it. Findings in `tui/`/`test/` can be fixed directly per the normal whitelist, but
  still call out anything non-trivial (behavior change, suppression, re-architecture) before
  applying it.
- **Daily operations:** Treat a `tools/run-clang-tidy.sh` pass (or at least a `-Wall -Wextra`
  build check) as a normal part of working in this repo, not a one-off cleanup — new findings on
  touched files should get surfaced and discussed in the same session, not left to accumulate.
- **`.clang-tidy` scope:** The current checks list deliberately excludes `cppcoreguidelines-*`
  wholesale (it fights this codebase's conventions: public POD-style members, short ISA names,
  raw pointer arithmetic on VRAM) and a few noisy `readability-*` checks. Expect to tune this
  file together as real findings come in rather than enabling everything at once.
- **Edit-domain exception:** `CMakeLists.txt`, `.clang-tidy`, and `tools/` were set up as a
  one-time exception to the whitelist in §1 for this tooling task. The whitelist itself (only
  `tui/`, `test/`, `docs/`) is unchanged for everything else — ask before assuming further root
  or build-system edits are pre-approved.

## 6. Hardware & ISA Specifications (Context)
- **Word Size:** 32-bit.
- **Memory:** Flat model, 4-byte instructions.
- **Addressing:** PC-relative for control flow; supports Position Independent Code (PIC).
- **Immediates:** 16-bit payload (`0x0000`-`0xFFFF`).
- **Graphics:** 320x240 @ 32bpp. Pitch is 1280 bytes. VRAM is memory-mapped — avoid touching
  low-memory addresses reserved for code/tables.
- **ALU Flags:** Z (Zero), N (Negative/Sign), V (Overflow), C (Carry). Signed logic relies on N
  (`BPL`/`BMI`).

See `docs/developer_guide.md` for the full architecture reference (memory map, stack frame layout,
instruction encoding, compiler pipeline) and `docs/commenting_standards.md` for the Doxygen
comment conventions used throughout the codebase.
