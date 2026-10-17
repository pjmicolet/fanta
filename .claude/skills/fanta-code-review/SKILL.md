---
name: fanta-code-review
description: Strict, project-specific compliance checklist for reviewing Project Fanta C++23 code (compiler, VM/CPU emulator, common headers, TUI). Use this whenever reviewing a diff, PR, or new implementation in this repo — it checks naming conventions, C++23 idioms, zero-virtual/hot-path allocation rules, and Fanta-specific hardware/ISA constraints (opcode shifting, 16-bit immediates, PC underflow, VRAM bounds, hex literals) that a generic review would miss. Mirrors the checklist the user already runs through Gemini via .agents/AGENTS.md, so use it to keep reviews consistent across assistants.
---

# Fanta Code Review

Run through this checklist and produce a compliance report, flagging violations with file:line
references. This is a strict, forward-thinking reviewer persona for Project Fanta's hardware, ISA,
performance, style, and C++23 guidelines — not a generic linter pass.

## 1. Codebase Architecture & File Structure
- The codebase is intentionally **flat** (flat `compiler/`, `vm/`, `tui/`, `common/` directories).
  Challenge any new nested subdirectory or complex hierarchy unless explicitly justified by scale.
- Flag cyclic header dependencies, redundant `#include`s, or files that mix unrelated
  responsibilities.

## 2. Readability, Naming, & Self-Documentation
- **Structs/Classes/Types:** `PascalCase` (e.g. `SimpleIRPass`, `Allocator`, `IROp`).
- **Methods/Functions/Variables:** `camelCase` (e.g. `assignFunc`, `outputIR`, `virtToReg`).
- **Enums/Macros/Constants:** `UPPER_SNAKE_CASE` (e.g. `THREE_OP`, `ARITH_ADD`).
- **No magic numbers:** opcodes, VRAM pitches, register limits, stack frame sizes must be named
  `constexpr` constants or enums, not raw literals. Any bitwise mask/shift needs a comment
  explaining the bit layout.
- **Clarity over cleverness:** challenge template metaprogramming or macro magic that's hard to
  reason about — prefer C++23 `if constexpr`, `concepts`, or `std::variant`/`std::visit`.
- **Comments explain why, not what** — hardware/emulator quirks, not restating the code. See
  `docs/commenting_standards.md` for the Doxygen tag conventions (`@brief`, `@note`, `@warning`,
  `@todo`) used in this codebase.

## 3. Modern C++ (C++23) & Idioms
- Prefer `std::expected`/`std::optional` over error codes or exceptions for expected-failure paths
  (parser errors, assembly failures).
- Prefer `std::println`/`std::print` (`<print>`) over `<iostream>` or `printf`-style output.
- Use monadic ops (`.and_then()`, `.transform()`, `.or_else()`) where they read cleanly.
- Use `std::concepts` to enforce compile-time interfaces instead of template-metaprogramming
  hacks or runtime checks.
- Challenge C-style pointer manipulation, raw arrays, manual resource management — prefer smart
  pointers, `std::span`, `std::string_view`.
- Challenge unnecessary copies — check `const&`, `std::move`, perfect forwarding are used where
  they should be.
- Challenge verbose loops where `std::ranges`/`std::views` or structured bindings would be
  cleaner.

## 4. Performance & Inefficiency Checks
- **Zero-virtual rule:** core compiler, register allocator, and CPU emulation logic must not
  declare `virtual` functions or rely on dynamic dispatch — use `std::variant`/`std::visit` (with
  the `overloaded` helper pattern) or templates instead.
- **Allocation auditing:** check hot paths (CPU execution step, tokenization, allocator passes)
  for heap allocations inside loops. Suggest `reserve()` or container reuse instead of
  reallocating.
- Prefer value semantics and small-object optimization.
- In CPU instruction decoding and emulator loops, keep decoding lean and branching minimal.

## 5. Hardware & Memory Constraints
- **Stack offset positivity:** memory decoding does straight addition of 16-bit immediates with no
  sign extension — stack offsets (eviction/reload) must always be positive.
- **VRAM boundaries:** VRAM is memory-mapped at 320x240 @ 32bpp, pitch 1280. Verify graphics logic
  can't overflow VRAM or touch low-memory code/table addresses.
- **PC underflow:** any PC arithmetic that looks up the previous cycle's PC must guard `PC < 4` to
  avoid out-of-bounds reads.

## 6. Instruction Set & Opcodes
- **Opcode shifting:** instruction opcodes must be shifted left by exactly 26 bits (`op << 26`).
- **Literal base:** numeric literals in assembly (after `#` or `$`) are hexadecimal by default.
- **Immediates range:** immediate fields must fit strictly within a 16-bit payload
  (`0x0000`-`0xFFFF`); larger constants need `LSH`/`ADD` construction.
- **Mnemonic standard:** mnemonics must be normalized to UPPERCASE before Registry lookup/parsing.
- **Surgical ISA changes:** never alter the contents of `static constexpr auto emit()` functions in
  instruction definitions — these are hand-optimized by the user. Flag any diff that touches one.

## Ownership boundary (see CLAUDE.md)
`compiler/`, `vm/`, and `common/` are user-owned — review and report findings there, but do not
edit those files directly. `tui/`, `test/`, and `docs/` are fair game to edit directly when fixing
review findings, if asked to apply fixes.

## Output format
Report findings grouped by the six sections above (skip sections with nothing to report). For each
finding: file:line, what's wrong, which rule it violates, and a concrete fix. End with a one-line
overall verdict (compliant / needs changes).
