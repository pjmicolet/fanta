# Fanta Adaptive CPU

**Status:** draft, 2026-11-07. Nothing here is implemented. Sections 1 and 2 are agreed; the rest is
a starting point that the experiments in section 6 should test.

## 1. Concept

The ISA stays fixed and the microarchitecture adapts. A unit inside the CPU observes running code
and synthesises faster internal versions of hot instruction sequences. Programs never name these.
The language side is in `docs/language_design.md`.

### Contract with the language

The two sides share only:
1. **The ISA.**
2. **Hints:** a `HINT` instruction, treated as a `NOP` by a CPU without the unit. A hint states a
   fact and may be ignored. **A wrong hint can never change what a program does.** The language
   ships with no hints until experiments show one helps.
3. **The guarantee:** with the unit on or off, a program produces the same results. Only its cycle
   count may change.

## 2. Safety rules and contract with the kernel

The CPU owns its slots and decides what to synthesise. The kernel never has to manage them, but it
does control whether the unit runs:

1. **Off at reset:** the kernel starts on plain instructions and enables the unit after setup.
2. **Never in kernel mode:** the unit does not observe, tune or run blocks for kernel code, and a
   trap enters plain execution. Exception handling therefore never depends on the unit. Kernel code
   gets no speedup, by design.
3. **Kill switch:** the kernel can disable or flush the unit at any time, through a privileged
   instruction or control register. It flushes on context switch, so a process never runs another's
   blocks.
4. **Faults roll back:** if a block faults, nothing commits. The CPU re-runs the original
   instructions unoptimised and reports the fault only if it recurs. The kernel sees only faults
   the original program would have caused.
5. **No stores in the first version:** rules 1-3 protect kernel code, not kernel data, and a bad
   block would not trap. Without stores, a bug can only corrupt the running program's own state.
   Allowing stores needs the same protection checks as normal instructions.

Requires the user/kernel mode bit in `docs/interrupts_proposal.md` section 4. The protection scheme
itself is undecided.

## 3. The unit (starting point)

**Pipeline:**
1. **Observe:** count back-edges and block executions.
2. **Select:** pick a hot subgraph, using a simple threshold at first.
3. **Install:** write it to one of N slots as a small dataflow block with atomic commit.
4. **Dispatch:** match on PC without modifying program memory, so disassembly stays truthful.

**Block limits, first version:** register and ALU operations, plus at most one final load.
- A load at or above `bus.lowest` reaches a device, so the block must fall back to plain execution.
- Final flags must match what the original instructions would produce.

**Costs:**
- **Cycles:** a block costs its depth, not its operation count.
- **Never slower:** installation runs in the background without stalling, so the plain timing is a
  true upper bound. Only slot count, block size and installation delay limit the unit.
- **Interrupts:** blocks are atomic, so the longest block bounds the added interrupt latency.

**Invalidation:** a store into synthesised code or a TUI live patch drops the entry. Use a coarse
filter, such as a per-page flag, so ordinary stores pay little.

## 4. Fitting it into the VM

- Add the unit as a second template parameter on `BaseCPU`, beside the bus, defaulting to `NoAdapt`:
  no virtual calls and no cost when off.
- Do not add a check to every fetch. Checking only on taken branches is the first thing to try.
- Today one instruction costs one cycle, and VBLANK fires every 50,000 instructions
  (`vm/cpu.cpp`). Blocks need VBLANK driven by cycles instead.
- Keep fictional cycle savings separate from host speed. The unit could also act as a VM JIT, but
  that must not drive the design.

## 5. Testing

- **Differential:** run a program with the unit on and off, and compare architectural state at block
  boundaries, building on `fanta-diff`.
- **Checking mode:** the VM runs each block both ways and stops on any mismatch. This catches silent
  wrong values, which rollback cannot.
- **Before a kernel exists:** a VM flag starts test programs in user mode with the unit on, because
  rule 2 would otherwise keep it from ever running.

## 6. Experiments, in order

1. **Measure the opportunity, with no unit.** Profile `test/vblank_*.asm` for how much dynamic
   execution falls in eligible register-only sequences and their achievable depth. If memory
   operations dominate, the block limits must change before anything is built.
2. **Hand-specified blocks.** Install blocks from a table and test dispatch, costs and the guarantee.
3. **Automatic selection.** Add the observer and threshold.
4. **Hints.** Add one only if a measured case needs a fact the hardware cannot observe, such as an
   upcoming behaviour change, aliasing or a stable value.

## 7. Open questions

- Should `HINT` cost a cycle? A `NOP` does today, so hints belong at region entry, not inside hot
  loops.
- Should some hot sequences stay out of the ISA on purpose? There is no `MUL` or `DIV`, so
  shift-and-add loops could be early targets.
- Which instruction or register exposes the kill switch?
- How many slots, and how large a block?
- Should later versions learn their selection policy, as with a small linear model on counters?

## 8. Related work

- [Transmeta Code Morphing](https://courses.cs.washington.edu/courses/cse548/08wi/papers/transmeta.pdf):
  thresholds, a translation cache, recompiling after faults.
- [Clark et al., CCA](https://www-users.cse.umn.edu/~zhai/courses/5980/readings/lec12/ntclark-micro04.pdf):
  collapsing subgraphs onto a feed-forward accelerator.
- [DySER](https://research.cs.wisc.edu/techreports/2010/TR1683.pdf): datapaths specialised at runtime.
- [Micolet, 2019](https://era.ed.ac.uk/server/api/core/bitstreams/dd968f06-41cb-457e-92b0-5de64e83496f/content):
  hardware-sized runtime models and reconfiguration cost, on core composition.
