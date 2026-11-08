# Not Implemented

Features the toolchain doesn't have yet, so nobody mistakes a gap for a bug or forgets it exists.
Crashes and wrong results with a known cause go in `known_bugs.md` instead. When a feature lands,
delete its row; git keeps the history. This file holds the facts; the work lives in a GitHub
issue, so link it (`#N`) in the row when one exists.

To re-validate a row, run its check and update **Last checked**. Rows that compile can be checked
with `./build/fanta-compiler c.fn -o c.bin`. A row that no longer holds should be deleted, or moved
to `known_bugs.md` if the behaviour is now simply wrong.

## Semantic analysis

The compiler has no semantic pass, so all of these compile without an error.

| Gap | Check | What happens today | Added | Last checked |
|---|---|---|---|---|
| Type checking of initialisers | `let a: float = 1;` | Accepted | 2026-11-07 | 2026-11-07 |
| Type checking of void used as a value | `let x: int = @print(72);`, or the same with a `void` function | Accepted; `x` gets whatever is in R0 | 2026-11-07 | 2026-11-07 |
| Return type checking | `fn main() -> void { return 1; }` | Accepted | 2026-11-07 | 2026-11-07 |
| Missing return checking | `fn f() -> int { }` | Accepted | 2026-11-07 | 2026-11-07 |
| Arity checking, too few arguments | `@print();`, or `f();` for `fn f(a: int)` | Accepted; a callee reads whatever is on the stack | 2026-11-07 | 2026-11-07 |
| Arity checking, too many arguments | `@print(72, 105);`, or `f(1, 2);` for `fn f(a: int)` | Accepted; `@print` ignores the extra argument | 2026-11-07 | 2026-11-07 |

`Hook.arity` in `common/hook.hpp` is declared for the arity check but nothing reads it yet.

## Intrinsics

| Gap | Check | What happens today | Added | Last checked |
|---|---|---|---|---|
| Return values | `emitCall` in `compiler/SimpleIRPass.cpp` | Never sets `CallFunc::dest`, and no hook writes a result | 2026-11-07 | 2026-11-07 |
| Loading code away from address 0 | `link()` in `compiler/instruction_emit.cpp` | The hook offset is `addr - (idx * 4)`, which only reaches the hook if the program loads at 0 | 2026-11-07 | 2026-11-07 |

## Diagnostics

| Gap | Check | What happens today | Added | Last checked |
|---|---|---|---|---|
| Source lines on tokens | `TODO: Add line` in `compiler/lexer.hpp` | Tokens carry line 0 | 2026-11-07 | 2026-11-07 |
