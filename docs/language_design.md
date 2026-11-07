# Fanta Language Design

**Status:** draft, 2026-11-07. This is the planned language; `docs/parser.md` describes what is
implemented today. Syntax is agreed direction unless listed under "Defaults to challenge".

## 1. Identity and scope

Fanta is a small, C++-minded systems language for a fantasy machine. It will never target real
hardware, and memory safety in the Rust style is a non-goal. Its interest comes from
metaprogramming: compile-time code runs on the Fanta CPU itself.

**The split:** the language owns `comptime` and metaprogramming. The CPU owns runtime discovery and
reconfiguration, specified in `docs/adaptive_cpu.md`. The language knows nothing about how the CPU
reconfigures itself.

**First goal:** a small kernel running a VBLANK demo.

**Rule:** design a feature only once the kernel or demo needs it, or a CPU experiment shows it
pays off.

## 2. Core language

```fanta
struct Uart { data: u32, status: u32 }               // fields stay in declared order
const UART = 0x0200_0000 as *device Uart;            // illustrative register layout

struct Task {
    sp:    u32,
    next:  *Task,
    entry: fn(),
}

fn putc(c: u32) {
    UART.data = c;
}

fn run_all(head: *Task) {
    let t = head;
    for (; t != null; t = t.next) {
        t.entry();
    }
}
```

| Area | Decision |
|---|---|
| Integers | `u8`-`u32`, `i8`-`i32`; `int` aliases `i32` |
| Removed for now | `float`, `string`, `long` |
| Inference | `let x = 10;`; annotations optional |
| Variables | `let` is mutable; `const` is known at build time |
| Pointers | `*T`, `&x`, `*p`; `p.x` auto-dereferences; `null` |
| Pointer arithmetic | `p + 1` scales by element size |
| Casts | `x as T`; no implicit conversions |
| Literals | Add `0x...` and `_` separators |
| Arrays | `[N]T`; slices `[]T` later |
| Functions | `-> void` optional; `fn(...)` is a type |
| `if` | Parentheses stay, avoiding ambiguity with struct literals |
| Blocks | No `;` after `if`/`for` blocks |
| Loops | `for (init; cond; step)`, with every part optional; `while` comes later as a convenience |
| Assignment | A statement, not an expression |

### `*device T`

Replaces `volatile`. `device` is a keyword only directly after `*` in a type, so `let device` stays
legal.

- Each access happens exactly once, at the written width: never removed, merged or split.
- Device accesses stay in program order, and ordinary memory accesses do not move across them.
- Converting between `*T` and `*device T` always needs `as`.
- It implies nothing about atomicity or synchronisation.
- Data shared with handlers is a separate problem, not solved by this keyword. VRAM is RAM, so
  whether it needs its own rule waits on how frames reach the screen.

## 3. Metaprogramming

One model: `comptime` functions, with `quote` for code generation. A macro is a compile-time
function that returns `Code`. This replaces the separate `macro`, `AST.create_*` and compiler-hook
drafts; a small AST API remains for walking and inspecting code.

```fanta
comptime fn CircularBuffer(T: type, size: int) -> type { ... }

comptime fn trace(e: Code) -> Code {
    return quote { let v = $e; @print(v); v };
}
```

**Execution:**
- The compiler compiles `comptime` functions to Fanta code and runs them on a VM linked into the
  compiler, compiling dependencies on demand. Dependency cycles are compile errors.
- Types and AST nodes are integer handles. `@` calls query the compiler, reusing the arena's
  `NodeIndex` design.
- Each evaluation has a cycle limit.
- Results are copied into the program image, with pointers relocated using type information.
- The adaptive unit is off, so builds are deterministic.
- Host and target are the same machine, so no cross-compilation mismatch exists.
- Expansion can be stepped through in `fanta-tui`.

`Code` exists only at build time; there is no compiler in the kernel.

## 4. What the kernel needs

Beyond section 2:
- Inline asm and trap-entry functions.
- Section placement and no hidden runtime.
- A compile-time user/kernel mode.
- The cooperative `interrupt`, `yield`, `handle` and `quiet` constructs stay as specified in
  `docs/cooperative_interrupts.md`.

**ISA changes this requires:**
- Register-indirect call. `JMP` and `CALL` take immediates only.
- `LOADB`/`STOREB`, proposed so `u8` does not need read-modify-write. RAM access is 32-bit today.
- A user/kernel mode bit (`docs/interrupts_proposal.md` section 4).

The protection scheme is undecided and does not affect syntax.

## 5. Implementation order

1. Semantic pass and real types. Types are strings on AST nodes today, and everything below needs
   this.
2. Pointers, structs, `*device`, casts, literals.
3. `const` evaluation on the VM.
4. Generics through `comptime` parameters.
5. `quote` and reflection.
6. Kernel.

## 6. Defaults to challenge

| Question | Default | Alternative |
|---|---|---|
| Blocks as expressions? | Yes; makes `quote` practical at some IR cost | Statements only |
| Kernel errors | Multiple returns, `-> (u32, Err)` | Return codes, or error unions |
| Generics | `comptime` parameters, with no template syntax | Add specialisation or concepts |

**Open:** how `*` and `/` lower, since the ISA has no `MUL`/`DIV`.

## 7. Parked

No design work until the rule in section 1 is met.

- **Effects for control:** treat `yields VBLANK`, `quiet`, syscalls and faults as one effect system.
  Depends on `handle`/`quiet`. See [Lexa](https://2025.splashcon.org/details/OOPSLA/203/Zero-Overhead-Lexical-Effect-Handlers)
  and [staged handlers](https://ps.cs.uni-tuebingen.de/publications/schuster19zero/).
- **Cycle budgets:** `fn f() budget(200)`, checked for bounded code. Needs a defined cycle model.
- **`@measure`:** build-time tuning against real cycle counts, in the spirit of
  [Terra](https://www.doi.org/10.1145/2491956.2462166).
- **Hot-path checks:** a `comptime` function walks `Code` and rejects things like division.
- **Layouts as libraries:** `soa()` as a `comptime` type function, and a good test of the model.
- **Board description:** a machine described in Fanta, with `device uart: Uart at 0x0200_0000`.
- **Live redefinition:** language-level hot patching in debug builds, extending the TUI's patching.
- **Traits:** only if `comptime` duck typing proves insufficient.
- **Compiler in the kernel:** would make `Code` usable at runtime.
