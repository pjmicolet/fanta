# Fanta Low-Latency Architecture Design Notes

These design notes explore alternative theoretical paths for **Project Fanta** to move away from classic MIPS/C clones and instead focus on modern concepts inspired by low-latency engineering (HFT), cache-preserving cooperative interrupts, and static metaprogramming.

---

## 1. Zero-Jitter Cooperative Interrupts (`yield` Points)

Traditional hardware interrupts asynchronously preempt the running application, resulting in unpredictable CPU context-switch latency, pipeline flushes, and L1/L2 cache thrashing.

### The Proposal:
Keep preemptive interrupts in the kernel, where they cost only a few instructions (set a pending bit, return), and deliver every event to user code through compiler-assisted cooperative yielding. User code never runs a handler at a point it didn't choose. Specified in `docs/cooperative_interrupts.md` (user layer, including the planned `handle` / `quiet` constructs) and `docs/interrupts_proposal.md` (kernel layer).

```fanta
// Bind interrupt handlers directly in user-space
interrupt VBLANK {
    draw_frame();
}

fn main() {
    let price : int = 100;
    loop {
        price = poll_feed();
        
        // Compiler-known safe yield point. 
        // Only active registers (e.g. price) are saved/pushed.
        yield VBLANK;
    }
}
```

### Low-Latency Advantages:
* **Surgical Stack Pushes**: The compiler knows exactly which registers are live at the `yield` statement. It only emits `PUSH`/`POP` instructions for those registers, reducing context-switch overhead to the absolute minimum.
* **Cache Integrity**: Cache lines remain warm because handlers only run at designated, optimized boundaries. The kernel's preemptive path is kept tiny, so it barely disturbs them.

---

## 2. Metaprogramming & Static Dispatch

Traditional object-oriented inheritance relies on dynamic dispatch (vtables), which requires a double-pointer dereference and prevents inlining. Fanta resolves polymorphism entirely at compile-time.

### First-Class `comptime` Types
Treat `type` as a first-class value that can be passed to and returned from functions at compile-time:

```fanta
fn CircularBuffer(comptime T: type, comptime size: int) type {
    return struct {
        data: Array(T, size),
        head: int,
        tail: int,
        
        fn push(self, val: T) {
            self.data[self.head] = val;
            self.head = (self.head + 1) % size;
        }
    };
}
```

### Monomorphized Traits (Static Interface Resolution)
Instead of run-time interface lookups, compiler traits generate concrete function duplicates for each type:

```fanta
trait Decoder {
    fn decode(self, bytes: Span(byte)) -> Message;
}

// Monomorphized at compile-time:
fn parse_feed(decoder: impl Decoder, bytes: Span(byte)) {
    let msg = decoder.decode(bytes); // Fully inlined direct call
}
```

---

## 3. Data Layout Optimization: Native SoA (Structure of Arrays)

In memory-bound loop iterations, Array of Structures (AoS) layouts cause unnecessary cache line loads. A Structure of Arrays (SoA) layout ensures that similar fields are stored contiguously in memory.

Fanta supports automatic compile-time layout transformations of arrays:

```fanta
struct OrderBookRow {
    price: int,
    volume: int,
    orders: int,
}

// Compiler transforms the array layout into 3 separate contiguous memory buffers
let book : SoA(OrderBookRow, 128);

fn clear_volume() {
    for i in 0..128 {
        book[i].volume = 0; // Compiles to a sequential memory write to book.volume array
    }
}
```
This guarantees maximum L1/L2 cache line utilization during loop updates.

---

## 4. Lisp-Like Metaprogramming & AST Splicing

To support deep syntax extensibility without dynamic overhead, Fanta supports Lisp-style macro expansion where code is treated as a compile-time data structure (Homoiconicity).

### Hygienic Quasi-Quoting
Create AST templates and splice user-space code expressions using the unquote (`$`) syntax:

```fanta
macro debug_assert(expr: Code) -> Code {
    return quote {
        if !$expr {
            panic("Assertion failed: $expr"); // splices AST
        }
    };
}
```

### Programmatic AST Construction
Expose the compiler's abstract syntax tree structures directly in `comptime` to allow programmatic function generation:

```fanta
comptime {
    let myFunc = AST.create_function("generated_func", type_int);
    myFunc.add_param("x", type_int);
    myFunc.set_body(
        AST.create_binary_op(OpCode.LSH, AST.ident("x"), AST.literal(3))
    );
    AST.inject(myFunc); // Injects generated AST into active compiler module
}
```

### Compiler Hooks (AST Analysis and Rewriting)
Write macros that act as compiler validation passes to enforce low-latency bounds (e.g. banning division or heap allocation on critical paths):

```fanta
macro restrict_hotpath(func: Code) -> Code {
    comptime {
        for node in AST.walk(func) {
            if node.type == NodeType.Division {
                Compiler.error("Division detected in hotpath! Cycle latency is too high.");
            }
        }
    }
    return func;
}
```
