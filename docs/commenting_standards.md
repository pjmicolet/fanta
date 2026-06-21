# Fanta C++ Commenting & Documentation Standards

While C++ doesn't have a single built-in documentation tool like Java's Javadocs, the industry standard is **Doxygen**. Doxygen uses a structured comment block format that modern editors (VS Code, CLion, Neovim) parse natively to provide hover tooltips, parameter hints, and autocomplete descriptions.

---

## 1. Doxygen Block Structure

Use the `/** ... */` block prefix for classes, structs, functions, and public interfaces.

### Function Documentation Template
```cpp
/**
 * @brief A short one-line description of what the function does.
 * 
 * @details Optional longer description detailing the logic, design decisions,
 *          or constraints.
 * 
 * @param paramName Description of the parameter and any preconditions (e.g. range, non-null).
 * @return Description of the returned value.
 * @throws ExceptionType Description of when this exception is thrown.
 */
```

### Example: Function Documentation
```cpp
/**
 * @brief Allocates physical registers to virtual operands and inserts stack spills.
 * 
 * @details Iterates over function instructions, mapping infinite virtual registers
 *          down to physical registers (R0-R14). Spills excess variables to stack offsets
 *          relative to FP if register pressure is exceeded.
 * 
 * @param function The input FunctionIR with virtual operands.
 * @return A new FunctionIR with virtual registers fully mapped to physical registers.
 */
auto assignFunc(const FunctionIR &function) -> FunctionIR;
```

---

## 2. Documenting Invariants & Structures (Classes / Structs)

Document custom types, tracking what they represent and how they are aligned.

```cpp
/**
 * @brief Metadata tracking a global variable's offset and type info.
 * 
 * @note All global variables are laid out contiguously starting from the linked
 *       global base address, aligned to 16-byte boundaries.
 */
struct GlobalVarInfo {
  uint32_t offsetFromBase; ///< Byte offset from the start of global VRAM segment
  uint32_t address;        ///< Absolute resolved memory address
  std::string_view rt;     ///< String name of the variable's type (e.g., "int")
};
```
> [!TIP]
> Use `///<` to document members inline on the same line as shown above.

---

## 3. Inline Comments: "Why", Not "What"

Per guidelines: *“Comments should explain the why (e.g., hardware/emulator quirks) rather than the what.”*

### ❌ What not to do (Redundant Comments):
```cpp
// Check if PC is less than 4
if (PC < 4) {
    return 0; // Return 0
}
```

###  What to do (Explaining Hardware Context):
```cpp
// PC underflow protection: the emulator reads instructions in 4-byte steps.
// Prevents out-of-bounds reads during branch trace calculation.
if (PC < 4) {
    return 0;
}
```

---

## 4. Tag Registry for Fanta Codebase

Use these standardized Doxygen tags to draw attention to codebase quirks:

* **`@note`**: Documents hardware details or architectural constraints (e.g. PC-relative addressing).
* **`@warning`**: Warnings about safety issues (e.g., memory alignment, VRAM overflows).
* **`@todo`**: Details features yet to be built (e.g., return value stack passing).

### Example with Standard Tags:
```cpp
/**
 * @brief Emits a relative jump instruction to the given target address.
 * 
 * @warning The target offset must be 4-byte aligned to match word boundaries.
 * @note Signed jumps use PC-relative addressing computed from current PC + 4.
 * @todo Support indirect jumps via registers in a future pass.
 */
```
