# Fanta Low-Latency Design Notes

**Status:** rationale only. The specs are `docs/cooperative_interrupts.md` (user layer),
`docs/interrupts_proposal.md` (kernel layer) and `docs/language_design.md` (language).

## 1. Zero-jitter cooperative interrupts

Traditional hardware interrupts preempt the application asynchronously, which causes unpredictable
context-switch latency, pipeline flushes and cache thrashing.

Keep preemptive interrupts in the kernel, where they cost only a few instructions (set a pending
bit, return), and deliver every event to user code through compiler-assisted cooperative yielding.
User code never runs a handler at a point it did not choose.

```fanta
interrupt VBLANK {
    draw_frame();
}

fn main() {
    let price = 100;
    for (;;) {
        price = poll_feed();

        // Compiler-known safe point: only live registers (here, price) are saved.
        yield VBLANK;
    }
}
```

- **Surgical stack pushes:** the compiler knows which registers are live at the `yield`, so it pushes
  and pops only those, keeping switch overhead to a minimum.
- **Cache integrity:** handlers run only at chosen boundaries, and the kernel's preemptive path is
  too small to disturb much.

## 2. Static dispatch

Vtables need a double dereference and prevent inlining, so polymorphism is resolved at compile time
through `comptime` parameters. Interrupt handlers follow the same direction: the compiler encodes
each handler directly in its `CIP`, with no table in memory.

## 3. Where the earlier ideas went

| Idea | Now |
|---|---|
| First-class `comptime` types | `docs/language_design.md` section 3 |
| Quasi-quoting with `quote` and `$` | `docs/language_design.md` section 3 |
| `macro` keyword | Replaced: a macro is a `comptime` function returning `Code` |
| `AST.create_*` construction | Replaced by `quote` |
| Hot-path checks through compiler hooks | Parked: a `comptime` function walking `Code` |
| Traits | Parked |
| Native SoA | Parked: a `comptime` library |
