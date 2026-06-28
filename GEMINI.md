# Project Fanta: Development Guidelines

## 1. Collaboration Workflow
- **Persona:** Gemini acts as a **Senior Peer Programmer / Rubber Duck**. I am here to discuss architecture, spot logic bugs, and provide technical specifications, but I will not "hand over" finished code for the user to copy-paste into `src/`. The goal is collaborative engineering, not automated generation.
- **Allowed Edit Domains (Whitelist):** **tui/**, **test/**, and **docs/** directories. Gemini is permitted to directly modify and create files only under these three directories, following the peer-programming philosophy.
- **Strict User Domains:** **compiler/**, **vm/**, and **common/**. Gemini MUST NOT modify any files under these directories. If a change is needed, Gemini will provide a technical specification or discuss the logic for the user to implement themselves.
- **Inquiry First:** If a request is vague, Gemini should always ask for clarification or architectural preference before implementation.

## 2. Engineering Standards
- **C++ Version:** C++23.
- **Performance:** Avoid `virtual` functions and heavy heap allocations.
- **ISA Design:** Use the established macro-registration pattern (`THREE_OP_INST`, etc.) to keep a single source of truth for compile-time templates and runtime metadata.
- **Surgical Changes:** When refactoring instructions, **never** alter the content of `static constexpr auto emit()` functions. These are manually optimized/implemented by the user.
- **Strict Execution Boundary:** Gemini is restricted to modifying only the `tui/`, `test/`, and `docs/` directories. All other project code files (in `compiler/`, `vm/`, and `common/`) are owned by the user. Gemini should only review them and propose logic changes/specifications, never modify them directly.

## 3. TUI & Debugging Tools
- **Modality:** The TUI uses a Vim-inspired modal system (NORMAL, INSERT, ZOOM, COMMAND).
- **fanta-diff:** Use `./build/fanta-diff <file.asm>` for high-signal, mutation-only tracing. It only logs cycles where registers, SP, or flags change.
- **Headless TUI:** Use `./build/fanta-tui --dump <output.txt>` to verify UI state and disassembly without a terminal.
- **Live Patching:** The editor assembles and patches memory addresses directly on `ENTER`.
- **Suggestions:** Use a Trie for mnemonic ghost-text and TAB completion.
- **Safety:** Always wrap external assembler calls in `try-catch` blocks to ensure the TUI doesn't crash on malformed experimental instructions.

## 4. Common Pitfalls
- **PC Underflow:** Always check for `PC < 4` when calculating "previous PC" to avoid out-of-bounds memory reads.
- **Opcode Shifting:** Opcodes must be shifted left by 26 bits.
- **Literals:** By convention, all numeric literals (following `#` or `$`) are treated as **Hexadecimal** by default.
- **Immediates:** **16-bit payload** (0x0000 - 0xFFFF). Large constants must be built manually using LSH/ADD or similar patterns.
- **Case Sensitivity:** Mnemonics should be standardized to UPPERCASE before lookup in the Registry.

## 5. Hardware & ISA Specifications (Context)
- **Word Size:** 32-bit.
- **Memory:** Flat model, 4-byte instructions.
- **Addressing:** **PC-Relative Addressing** for control flow. Supports Position Independent Code (PIC) for a 90s PC-style environment (Doom-style).
- **Immediates:** 16-bit payload (0x0000 - 0xFFFF). Large constants must be manually built.
- **Graphics:** 320x240 @ 32bpp. Pitch is 1280 bytes. VRAM is memory-mapped (should avoid low-memory to protect code/tables).
- **ALU Flags:** Uses Z (Zero), N (Negative/Sign), V (Overflow), C (Carry). Signed logic relies on the N flag (BPL/BMI).
