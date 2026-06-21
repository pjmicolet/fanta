# Fanta Architecture & Compiler Developer Guide

Welcome to the central developer guide for **Project Fanta**. This document maps out the system architecture, memory map, stack layouts, instruction formats, and compiler pipeline to serve as a reliable reference when returning to or scaling the codebase.

---

## 1. Hardware & CPU Specifications

The Fanta CPU is a custom 32-bit RISC processor designed for 90s-style execution environments.

```
                  +-----------------------------------+
                  |           Fanta CPU               |
                  +-----------------------------------+
                  | R0 - R14 : General Purpose        |
                  | R15      : Frame Pointer (FP)     |
                  | R16      : Stack Pointer (SP)     |
                  +-----------------------------------+
                  | ALU Flags: Z, N, V, C             |
                  +-----------------------------------+
                                    |
                  +-----------------+-----------------+
                  |                                   |
    +-------------+-------------+       +-------------+-------------+
    |        Memory Map         |       |      Graphics VRAM        |
    +---------------------------+       +---------------------------+
    | 0x000000 - 0x7FFFFF (8MB) |       | 0x800000 - 0xFFFFFF (8MB) |
    | Code & Stack Space        |       | 320x240 @ 32bpp           |
    +---------------------------+       +---------------------------+
```

### Key Specs:
* **Word Size:** 32-bit (4 bytes).
* **RAM Size:** 32MB flat memory.
* **Stack Position:** The Stack Pointer (`SP`/`R16`) starts at the top of the stack space (`0x7FFFFF`) and grows downward.
* **VRAM Mapping:** Standard VRAM starts at address `0x800000` (8MB boundary).
  * Resolution: 320x240 @ 32bpp.
  * Pitch: 1280 bytes per row.

---

## 2. Memory & Stack Frame Mechanics

Understanding the stack layout is critical for debugging code generation, register allocator spills, and compiler parameters.

### Function Call Sequence
1. **Caller** pushes arguments onto the stack in reverse order (`Param N` down to `Param 1`).
2. **Caller** executes `CALL`. This writes the return PC to the current `SP` and decrements `SP` by 4.
3. **Callee** executes prologue:
   * Saves the caller's Frame Pointer (`FP`/`R15`) via `PUSH FP`.
   * Saves all callee-saved registers utilized in the function (`PUSH Reg`).
   * Allocates local variable and spill slots on the stack by subtracting the frame size from `SP` (`SUB_IMM SP, SP, frameSize`).
   * Sets `FP` to the bottom of this newly allocated frame: `MOV FP, SP`.

### Stack Frame Layout (Visualized)
Below is the physical memory layout of a stack frame when FP is established:

| Address Offset | Memory Slot Content | Pushed By | Access Mode |
|:---|:---|:---|:---|
| `FP + FrameSize + SpillSize + 8` | **Param 2** | Caller | `[FP + offset]` |
| `FP + FrameSize + SpillSize + 4` | **Param 1** | Caller | `[FP + offset]` |
| `FP + FrameSize + SpillSize` | **Saved Return PC** | `CALL` | *Internal Ret* |
| `FP + FrameSize + SpillSize - 4` | **Saved Caller FP** | Callee | *Epilogue Pop* |
| `FP + FrameSize` | **Callee Register N** | Callee | *Epilogue Pop* |
| ... | ... | Callee | ... |
| `FP + 4` | **Local / Spill Slot 1** | Callee | `[FP + stackOffset]` |
| `FP + 0` | **Local / Spill Slot 2** | Callee | `[FP + stackOffset]` |

> [!NOTE]
> All stack frame offsets are in **bytes** and must be 4-byte aligned. The stack offset for local variables runs from `FP + 4` upward to `FP + FrameSize`.

---

## 3. Instruction Encoding & Formats

Instructions are exactly 32-bit (4 bytes) wide. Opcodes are shifted left by **26 bits** (`opcode << 26`).

```
Standard formats:
1. THREE_OP (Register):  [Opcode: 6] [Dest: 5] [Src1: 5] [Src2: 5] [Unused: 11]
2. THREE_OP (Immediate): [Opcode: 6] [Dest: 5] [Src1: 5] [Immediate: 16]
3. TWO_OP (Register):    [Opcode: 6] [Dest: 5] [Src1: 5] [Unused: 16]
4. TWO_OP (Immediate):   [Opcode: 6] [Dest: 5] [Immediate: 16]
5. SINGLE_INST / JUMP:   [Opcode: 6] [Address/Offset: 26]
```

### Common Opcodes Registry (Excerpt)
| Mnemonic | Format | Register Opcode | Immediate Opcode | Description |
|:---|:---|:---|:---|:---|
| **ADD** | THREE_OP | `0x01` | `0x02` | Dest = Src1 + Src2 |
| **MOV** | TWO_OP | `0x03` | `0x04` | Dest = Src1 |
| **SUB** | THREE_OP | `0x05` | `0x06` | Dest = Src1 - Src2 |
| **STORE** | MEM | — | `0x08` | `[Dest_Base + Offset] = Reg_Val` |
| **LOAD** | MEM | — | `0x09` | `Reg_Val = [Src_Base + Offset]` |
| **CALL** | BRANCH | — | `0x15` | Call subroutine at absolute/relative PC |
| **RET** | RET | `0x16` | — | Return from subroutine |
| **PUSH** | STACK | `0x1D` | — | Push register onto stack, SP -= 4 |
| **POP** | STACK | `0x1E` | — | Pop stack to register, SP += 4 |

---

## 4. The Compilation Pipeline

The Fanta compiler target runs a flat multi-pass sequence to turn source `.fan` files into raw `.bin` executables.

```
 +-------------+       +-------------+       +-------------------+       +-----------------+
 |  1. Lexer   | ----> |  2. Parser  | ----> | 3. Global Extract | ----> | 4. SimpleIRPass |
 +-------------+       +-------------+       +-------------------+       +-----------------+
                                                                                  |
                                                                                  v
 +-------------+       +-------------+       +-------------------+       +-----------------+
 |  8. Output  | <---- |  7. Linker  | <---- |     6. Emitter    | <---- |  5. Allocator   |
 +-------------+       +-------------+       +-------------------+       +-----------------+
```

### Compiler Phases Explained:
1. **Lexer (`compiler/lexer.hpp`):** Tokenizes input code.
2. **Parser (`compiler/parser.hpp`):** Performs a recursive-descent parsing sequence to build an AST.
3. **Global Extractor (`compiler/codegen.cpp`):** Registers global functions and variables in the `GlobalTable` and calculates memory offsets for global variables prior to lowering.
4. **Lowering (`compiler/SimpleIRPass.cpp`):** Traverses the AST and emits `Virtual IR` using temporary/virtual registers. Evaluates global variables in the synthetic `__init` entry point function.
5. **Allocator (`compiler/allocator.cpp`):** Maps infinite virtual registers down to physical registers (0-14). Inserts stack spills (`LOAD` / `STORE` relative to `FP`) when register pressure is exceeded.
6. **Instruction Emitter (`compiler/instruction_emit.cpp`):** Emits concrete 32-bit instructions directly to a flat global list.
7. **Linker (`instruction_emit.cpp::link`):** Performs a final patch-up pass to resolve call targets (`CALL`) and global variable base offsets (`LocalGlobalBase` -> `MOV_IMM`).

---

## 5. Developer Modalities & Debugging

Fanta provides high-signal debugging tools:

1. **Modal TUI (`fanta-tui`):**
   * Vim-inspired modal environment: `NORMAL`, `INSERT`, `ZOOM`, `COMMAND`.
   * Live memory patching directly upon hitting `ENTER`.
2. **High-Signal Trace Diff (`fanta-diff`):**
   * Trace execution using: `./build/fanta-diff <file.asm>`.
   * Filters out constant cycles, only logging states where registers, stack pointer, or flags actually mutate.
3. **Headless Disassembly (`fanta-tui --dump`):**
   * Output disassembler and VM state directly to a file: `./build/fanta-tui --dump <output.txt>`.
