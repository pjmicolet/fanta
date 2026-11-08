# Plans

Work we've decided to do but haven't done yet, so it survives `/clear` and new sessions. Each
row points at where the detail lives; it doesn't restate it. TUI work goes in `tui_todo.md`,
bugs in `known_bugs.md`, and missing features with no decision yet in `not_implemented.md`.

Add a row whenever something is decided and deferred. When it lands, delete the row; git keeps
the history.

| Item | Decided | Spec | Next step | Who |
|---|---|---|---|---|
| Illegal opcodes trap through the kernel trap entry | 2026-11-08 | `interrupts_proposal.md` §8 | Settle where `TRAP_VECTOR` lives (§8 open question 1), then implement the minimal first cut | Owner, or a new session |
| Harden the parser: use the `expect()` results to report errors | 2026-11-08 | None yet | Write a spec. This also clears the last 41 clang-tidy findings (all in `parser.cpp`: unused `expect()` bindings, their dead stores, and `EXTRACT_TOKEN_TO_VAR` macro-parentheses false positives); `tools/tidy-summary.py` has them as its baseline | Owner |
| Consider strong `Addr`/`Reg`/`Word` types for the bus and ISA API, so swapped arguments (`store(val, addr)`, `parse_three` registers) fail to compile | 2026-11-08 | None yet; `bugprone-easily-swappable-parameters` was disabled for this, its 12 hits list the signatures | Design note first: must not cost anything in `emit()`/decode hot paths | Owner |
| Replace the `InstructionEmitter::emitInst` opcode switch with a format lookup through `kInstructionTable`, so a new opcode isn't also hand-added there | 2026-11-08 | CLAUDE.md §2 (ISA macro registration is the single source of truth); `NOLINT` block in `compiler/instruction_emit.cpp` | Own PR; check the lookup costs nothing at compile time | Owner |
