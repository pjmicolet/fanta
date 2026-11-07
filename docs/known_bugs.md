# Known Bugs

Check here before debugging a crash or wrong result. When you find a new root cause, add it. When
a bug is fixed, delete its row; git keeps the history.

## Reproduced on `main` at `9e351a1` (2026-11-07)

Crashes and hangs reproduce with `tools/crash.py fanta-compiler c.fn -o c.bin`. IDs come from
`docs/reviews/2026-10-17-compiler-vm-full.md` §2.

| ID | Input or trigger | Symptom | Cause | Where |
|---|---|---|---|---|
| New | `let b : int = ;` | Crash, or reports OK | Missing initialiser leaves an invalid node index | `SimpleIRPass::emitVariableIR`, via `Parser::getNodeAtIndex` |
| B11 | `let a : int = -1;` | Crash | Unary minus produces an invalid node index | `Parser::walkPrefix`, via `Parser::getNodeAtIndex` |
| B7 | A source file containing only `@` | Crash | Dereferences an errored `expected` | `Parser::Parser` |
| B6 | EOF inside a body; `fn main(1)` | Parser hangs | Parser loops without advancing | `compiler/parser.cpp` |
| B10 | `for (; i < 3; i = i + 1)` | `bad_variant_access` error | Empty init is read as a `VariableDecl` | `compiler/SimpleIRPass.cpp` |
| B4 | `2 * 3`, `6 / 3` | Program halts early, wrong result | Unsupported operators map to opcode 0 (HALT) | `getOpcodeFromString` |
| B3 | `let a : int = 70000;` | Value truncated to `0x1170` | Literals above `0xFFFF` aren't built up | `compiler/SimpleIRPass.cpp`, `compiler/instruction_emit.cpp` |
| New | `return qd;` with `qd` undefined | Compiles without error | Undefined variables aren't rejected | Compiler |
| New | Any compile | Prints `About to get stuck` | Leftover debug output | Compiler |
| New | Any compiled program | Emits HALT as `0x03FFFFFF`, so `:eject` misreads code | Should emit `Halt::emit()` (`0`) | Compiler |
| B16 | `SP` as a source operand | Encoded as immediate `#16` | Checks the first letter instead of `is_register()` | `parse_two`, `vm/string_assembler.hpp` |

## Not rechecked since 2026-10-17

B8, B9, B12, B13, B15, B17, B18 and B19 may still be open. See the review report above. Reproduce
one before relying on it, then move it to the table or drop it.

Rechecked and no longer reproducing: B1, B2, B5, and the statement-after-`return` case of B6.
B14 was also fixed: `inst_count` is now a CPU member.
