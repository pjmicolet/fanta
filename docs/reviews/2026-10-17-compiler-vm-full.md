# Review: compiler/ + vm/ (full pass) — 2026-10-17

Reviewers: `fanta-perf-reviewer`, `fanta-design-reviewer`, `fanta-test-reviewer`, run in
parallel and synthesised by the main session. Scope: `compiler/`, `vm/`, plus the `common/` headers
they include. Baseline: clang-tidy reports 682 unique warnings (183 designated-initializers,
92 signed-bitwise, 79 trailing-return-type, 52 use-emplace, …).

## 1. Summary

- **Most urgent: `CALL` and `JREL` are broken in the text assembler.** Commit `e464f13` (the
  registry static-init fix) put both in `kInstructionTable` as absolute `JUMP`. The macros
  (`BRANCH_INST(Call, …)`) and the VM's `Call::exec`/`Jrel::exec` treat them as PC-relative.
  Every `.asm` demo that calls a subroutine now jumps into zeroed memory and HALTs. Plasma halts at
  PC 0x330 after 50,035 cycles.
- The opcodes are kept by hand in about 7 places, and that is the root cause of the CALL bug.
  Design and perf independently proposed the same fix: one X-macro ISA list with a `static_assert`.
- The compiler fails silently in several ways: `*` and `/` compile to HALT, a lone `&` lexes as `*`,
  literals above 0xFFFF are truncated, the parser hangs on malformed input, and the 15th parameter
  and any parameter after a `long` are read from the wrong slot.
- VM: batching `run_cycles(n)` with a per-CPU VBLANK countdown is a small change and measures
  **2.1–2.6×** faster. Tail-call dispatch on top reaches **3.0–3.7×**.
- I added 31 tests that lock in current behaviour. The suite has 98 test cases and all pass. The
  14 failing tests the tester found are listed as probable bugs in §2, not added to the suite.

**Top 3 actions**
1. Fix `CALL`/`JREL` in `kInstructionTable` and add the guard `static_assert`s (§2 B1, D1/P6).
2. Move the initial SP below VRAM and word-align it (§2 B2).
3. Make compiler errors loud. Reject `*`/`/` and lone `&`/`|` until they're implemented, and add
   `Eof` checks to the parser loops (§2 B4–B7, D3).

## 2. Probable bugs

All of these are in user-owned code (`compiler/`, `vm/`, `common/`). None were fixed. The failing
test that reproduces each one is in §5.2.

| # | Bug | Location | Repro |
|---|---|---|---|
| B1 | `CALL`/`JREL` registered as absolute `JUMP`; assembler emits absolute targets, VM treats them as relative | common/instructions.hpp:200,211 | every vblank demo with `CALL` halts |
| B2 | SP starts at 0x7FFFFF (unaligned); first push writes bytes 0x7FFFFF–0x800002 = VRAM pixel 0 | vm/cpu.hpp:28,130 | T1 |
| B3 | Integer literals > 0xFFFF truncated by MOV_IMM (no LSH/ADD materialisation) | compiler/SimpleIRPass.cpp:181-187, instruction_emit.cpp:219 | T2 |
| B4 | `getOpcodeFromString` returns 0 (HALT) for `*`, `/` and anything else not + = - | compiler/SimpleIRPass.cpp:125-136 | T3 |
| B5 | Lone `&` / `\|` fall through to `case '*'` → `Mult` | compiler/lexer.hpp:198-218 | T13 |
| B6 | Parser hangs: statement after `return` (`continue` skips `;`), EOF inside a body, bad param list | compiler/parser.cpp:224,278-301 | T6 |
| B7 | Parser derefs an errored `expected` (`*lexer.getToken()`), which is UB on input like `@` | compiler/parser.cpp:8-9,188 | read |
| B8 | Param-offset patch loop bounded by `calleeRegs.size()`, not param count, so param 15 isn't re-based; it can also match spill loads | compiler/allocator.cpp:73 | T4 |
| B9 | Caller pushes 4 bytes per arg, callee prelude advances 8 for `long`/`float` | compiler/SimpleIRPass.cpp:501-508 vs 592-620 | T5 |
| B10 | `for (; …)` with no init: `std::get<VariableDecl>` on node 0 throws | compiler/SimpleIRPass.cpp:82-84 | T11 |
| B11 | Unary minus: `walkPrefix` returns -1 → `BinaryOperator{lhs=SIZE_MAX}` | compiler/parser.cpp:357-386 | T12 |
| B12 | CMP computes `int32` `data - data2`, which is signed overflow UB, and never sets V. The result differs between -O0 and -O3 | common/instructions_impl.hpp:147-163 | T7 |
| B13 | LSH by ≥ 32 is UB; on arm64 `1 << 32` gives 1 | common/instructions_impl.hpp:77-78,204 | T14 |
| B14 | `inst_count` is a file-scope global, so VBLANK timing is shared between CPU instances (tests, TUI reset) | vm/cpu.cpp:5,18-21 | T8 |
| B15 | Assembler: imm > 0xFFFF silently masked, `R16+` → -1 → masked to R31 (OOB read of 17-entry `registers`), unknown labels default-insert 0 | vm/string_assembler.hpp:164-172 | T9 |
| B16 | Assembler: `SP` as a source operand is encoded as immediate #16 (`tokens[n][0] != 'R'`) | vm/string_assembler.hpp:221,233 | T10 |
| B17 | `Push`/`Pop`/`Cip` index arrays with the unmasked 26-bit field; `read32`/`write32` unchecked | common/instructions_impl.hpp; vm/cpu.hpp:7-15 | read |
| B18 | Constexpr assembler can't express imm 0x8000–0xFFFF: `Literal<int16_t>` vs `uint16_t Imm` specialisations, so `Literal<-1>` has no `emit()` | common/instructions.hpp:116 vs macros | compile |
| B19 | Frame `SUB/ADD SP, SP, #maxOffset` and global base MOV_IMM truncate past 16 bits | compiler/instruction_emit.cpp:40,116-118,163-165 | read |
| B20 | `CIP` has no `emitInst` case, so the compiler silently drops it | compiler/instruction_emit.cpp | read |

Minor items: `--dump-ir` prints register moves as `Mov #0` (SimpleIRPass.cpp:166, `s2type =
Immediate` on MOV_REG); `parse_as_signed` is non-`inline` in a header (an ODR break once a second
TU includes it); `vm/cpu.hpp` relies on getting `<array>`/`<cstring>` through other headers, and
`ir.hpp` relies on the same for `<optional>`. docs/interrupts_proposal.md assigns EI=0x1F and DI=0x20,
which collide with CIP and BNC.

**Decision needed:** identifiers can't contain `_` (`my_var` lexes as `my` followed by an
UnknownToken). Is that intended?

## 3. Conflicts and overlaps (perf vs design)

| Topic | Perf says | Design says | Recommendation |
|---|---|---|---|
| ISA source of truth | P6: X-macro list + `static_assert` (correctness, zero cost) | D1: same, plus it removes 7 hand-synced lists | **Agree.** Do this first. The cheapest step, `static_assert(Registry::fetch("CALL").fmt == BRANCH)` and cross-checks against `cpu_info.hpp`, can land today. |
| Threaded dispatch (P3) | +40–100% on top of P1 | Adds another opcode table to keep in sync, and `[[clang::musttail]]` isn't ISO C++ | **Wait for D1.** Once the handler table is generated from `FANTA_ISA`, the sync cost goes away. Keep the P1 switch as the portable path and for single-stepping. P1 alone already gives ~2.5×, and at 50k instr/frame the console only needs ~3 MIPS. |
| Constexpr template assembler | (neutral) | D10: costs a macro specialisation per format for one demo user (`line.hpp`) | **Your call.** CLAUDE.md names the macro pattern as the source of truth. Driving it from `FANTA_ISA` keeps it at zero per-instruction cost without deleting anything. |
| Allocator rewrite | P2: flat arrays, 3–4× faster, 10× fewer allocs | D2: explicit def/use operand slots + `LoadParam` node | **Do both together.** They touch the same functions. D2 step 1 (`LoadParam`) also fixes B8. |
| AST access | P5: `getNodeAtIndex` → `const&`, move FunctionIR | D5: parser exposes a `const Ast&` arena | **Agree.** P5 is the first step of D5. |
| IR labels | P8: integer `LabelId`s, IR 64 → ≤40 bytes | (readability) `printIR` needs a formatter | Low priority. Do it if you touch the IR for D2. |
| VBLANK counter | P1: per-CPU countdown member | D9: `inst_count` is a mutable global | **Agree.** This also fixes B14. |
| Register file | P7: pad `registers` to 32 so every 5-bit decode is valid | D8/B15: the assembler should reject R16+ | **Complementary.** Do both: the VM is safe against any encoding, and the assembler reports user errors. |

## 4. Findings

### Performance

Benchmarks: Release `-O3`, Apple clang 17, arm64, 200M instructions, best of 3. Every variant
produced an identical VRAM hash and PC.

| ID | Title | Sev | Evidence |
|---|---|---|---|
| P1 | Batch `run_cycles(n)` + per-CPU VBLANK countdown | high | plasma 213→543-560, starfield 243→631, bounce 243→627 MIPS |
| P2 | Allocator: `unordered_map` → flat `vector`/`array`; don't copy+clear `insts` | high | regalloc 90.7→22-28 ms, 1.43M→144k allocs (4.7 MB synthetic input) |
| P3 | Tail-call threaded dispatch reusing `Op::exec` | medium | +P1: plasma ~620, starfield ~895, bounce ~887 MIPS |
| P4 | Generated code is 48% PUSH/POP; dead duplicate epilogue; a frame slot for every vreg | medium | 233 of 486 instructions on sample; 8/36 dead in test_input.fn |
| P5 | `getNodeAtIndex` by value; FunctionIR/bodies copied instead of moved | medium | IR pass 448k→272k allocs, 55→40 ms |
| P6 | Runtime instruction table synced by hand (→ B1) | medium | guard `static_assert` fails on current tree |
| P7 | Pad registers to 32, mask memory addresses (perf-neutral, removes UB) | medium | within noise of P1 |
| P8 | `IRInst` is 64 bytes because of an unused `CallFunc::args` and `std::string` labels | low | `sizeof` measured |
| P9 | String assembler allocates per line and per label lookup; `operator[]` inserts unknown labels | low | read |

**P1: batch execution**
```cpp
// cpu.hpp: public: auto run_cycles(std::uint32_t n) -> void;
//          member: std::uint32_t cycles_to_vblank = CYCLES_PER_FRAME;  (drop global inst_count)
[[gnu::always_inline]] inline auto CPU::step() -> void {
  auto instr = fetch();
  switch ((instr >> 26) & 0x3F) { /* ...same EXEC_INST cases as today... */ default: break; }
}
auto CPU::run_cycles(std::uint32_t n) -> void {
  while (n != 0 && !halted) {
    const auto chunk = std::min(n, cycles_to_vblank);
    std::uint32_t i = 0;
    for (; i < chunk && !halted; ++i) step();
    n -= i;
    if ((cycles_to_vblank -= i) == 0) { cip_interrupts[0] = 1; cycles_to_vblank = CYCLES_PER_FRAME; }
  }
}
auto CPU::run_cycle() -> void { run_cycles(1); }   // TUI / fanta-diff / tests unchanged
// vm/main.cpp frame loop: cpu.run_cycles(CYCLES_PER_FRAME);
```

**P2: allocator state as flat arrays** (vregs are dense per function, pregs are 0–14)
```cpp
static constexpr std::uint32_t kNone = UINT32_MAX;
std::array<bool, 15> usedRegisters{};
std::vector<std::uint32_t> virtToReg;          // vreg -> preg | kNone; capacity reused across funcs
std::array<std::uint32_t, 15> regToVirt{};     // preg -> vreg | kNone
std::vector<std::uint32_t> virtToStackOffset;  // default 0, same as operator[] today

// assignFunc: don't deep-copy then clear insts
FunctionIR fir{function.params, function.localVarCount, function.calleeRegs, {}, function.name};
fir.insts.reserve(function.insts.size() + function.insts.size() / 2);
```

**P3: threaded dispatch** (after D1, so `kTable` is generated)
```cpp
using Handler = std::uint32_t (*)(CPU &, std::uint32_t inst, std::uint32_t left);
template <class Op>
std::uint32_t handler(CPU &cpu, std::uint32_t inst, std::uint32_t left) {
  Op::exec(cpu, inst);
  if (left == 0 || cpu.halted) [[unlikely]] return left;
  const auto next = cpu.fetch();
  [[clang::musttail]] return kTable[next >> 26](cpu, next, left - 1);
}
```

**P4: cheap codegen wins**
```cpp
// outputInstructionsForFunc, after the visit loop: skip the dead trailing epilogue
if (fir.insts.empty() || !std::holds_alternative<Return>(fir.insts.back()))
  emitEpilogue(fir, maxOffset, il);
// allocator: assign virtToStackOffset lazily on first spill, so frame = #spilled * 4.
// Further out: last-use pass + linear scan, so registers can be reused.
```

**P5: stop copying AST/IR**
```cpp
auto getNodeAtIndex(Fanta::AST::NodeIndex idx) const -> const Fanta::AST::AstNode &;
Parser::Parser(std::string program) : program_(std::move(program)), lexer(program_) { ... }
asts.push_back({Fanta::AST::FunctionCall{idx, std::move(args)}, 0});
const auto &bodyNode = std::get<AST::FunctionBody>(p.getNodeAtIndex(fdef.body).t);
irDef.functions.push_back(std::move(func));
// and take falseLabel/trueLabel as const std::string& (SimpleIRPass.hpp:44-45,69,76,79)
```

**P7: register file and memory bounds**
```cpp
struct Memory {
  static constexpr std::size_t kSize = 32u * 1024 * 1024, kMask = kSize - 1;
  Memory(std::size_t) : memory(kSize + 4) {}   // slack for a 4-byte access at the top
  auto read32(std::size_t a) -> uint32_t { uint32_t v; std::memcpy(&v, memory.data() + (a & kMask), 4); return v; }
};
std::array<std::uint32_t, 32> registers{};    // every 5-bit field is valid
// Push/Pop: inst & 0x1F
```
Use `kSize - 1` plus slack, not an aligning mask: starfield does unaligned stores, and an
aligning mask changed its output.

**P8: compact IR**
```cpp
using LabelId = std::uint32_t;
struct CallFunc { std::string_view name; std::optional<Operand> dest; };
struct Branch  { InstOp opcode; LabelId label; };
struct IRLabel { LabelId id; };
static_assert(sizeof(IRInst) <= 40 && std::is_trivially_copyable_v<IRInst>);
```

**P9: assembler label lookup**
```cpp
struct SvHash { using is_transparent = void;
  size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); } };
std::unordered_map<std::string, int, SvHash, std::equal_to<>> labels_;
if (auto it = labels_.find(token); it != labels_.end()) return is_jump ? it->second : it->second - address;
return -1;   // unknown label is an error, not 0
```

### Design

| ID | Title | Sev | Perf cost of fix |
|---|---|---|---|
| D1 | Opcodes hand-synced in 7 places, already drifted | high | none |
| D2 | IR operand slots change meaning per opcode | high | none (removes a post-pass) |
| D3 | Parse errors are dropped → hangs and OOB | high | none |
| D4 | SimpleIRPass: repeated signatures, duplicated call lowering, catch-alls hide missing cases | medium | none |
| D5 | Stage boundaries leak (common→vm, compiler→tui, IR pass→Parser) | medium | positive |
| D6 | 32-bit encoding layout re-implemented in 6 places | medium | none |
| D7 | Lexer: hand switch with fallthrough bug, per-token boilerplate | medium | none |
| D8 | Text assembler guesses operand kinds, uses -1 as its error value | medium | none |
| D9 | CPU state uses magic indices; SP overlaps VRAM | medium | none |
| D10 | Constexpr template assembler costs every ISA change, has one user | low | none at runtime |

**D1: one ISA list** (the `emit()` bodies stay untouched; only the list of macro invocations moves)
```cpp
// X(format, ClassName, "MNEMONIC", regOpcode, immOpcode)
#define FANTA_ISA(X)                        \
  X(THREE_OP,   Add,  "ADD",  0x01, 0x02)   \
  X(TWO_OP,     Mov,  "MOV",  0x03, 0x04)   \
  X(BRANCH,     Call, "CALL", 0x15, 0x00)   \
  X(JUMP,       Cip,  "CIP",  0x1F, 0x00)   \
  X(BRANCH,     Bnc,  "BNC",  0x20, 0x00)
#define FANTA_TABLE_ENTRY(fmt, cls, mn, reg, imm) InstEntry{mn, {fmt, reg, imm}},
inline constexpr auto kInstructionTable = std::to_array<InstEntry>({FANTA_ISA(FANTA_TABLE_ENTRY)});
namespace Op {
#define FANTA_OPCODE(fmt, cls, mn, reg, imm) \
  inline constexpr std::uint8_t cls = reg; inline constexpr std::uint8_t cls##Imm = imm;
FANTA_ISA(FANTA_OPCODE)
}
consteval auto opcodesUnique() -> bool { /* 64-entry seen[] over reg (+imm for 3/2-op) */ }
static_assert(opcodesUnique(), "two instructions share an opcode");  // also rejects EI=0x1F
// Templates: #define FANTA_TEMPLATE(fmt, cls, mn, r, i) FANTA_TEMPLATE_##fmt(cls, mn, r, i)
```
Smallest first step without the X-macro: build the VM case labels from `Registry::fetch("ADD").reg`,
and add `static_assert(Op::ADD_REG == Fanta::Info::Instructions::ADD_REG)` for each constant.

Adding an instruction touches these places today: (1) the `*_INST` macro, (2) the
`kInstructionTable` row plus the hard-coded size `25`, (3) `cpu_info.hpp`, (4)
`instructions_impl.hpp`, (5) the `vm/cpu.cpp` case (if it's missing, the instruction silently does
nothing), (6) `ir.cpp` `kOpTable`, (7) the `instruction_emit.cpp` case (if it's missing, the
instruction is silently dropped), (8) the `link()` branch list, (9) the `string_assembler`
format arm, (10) `tui/disasm.hpp`, (11) the lowering in SimpleIRPass. That is 8 files and 7
opcode lists. After D1 it is one row, the exec struct, and an optional lowering.

**D2: explicit def/use operand roles**
```cpp
struct Operand { std::uint32_t val; bool isVirtual; };
struct Imm { std::uint32_t val; };
using Src = std::variant<Operand, Imm>;
struct IROp {
  std::uint8_t opcode;                       // reg/imm form picked at emit from the Src kind
  std::optional<Operand> def;                // CMP / PUSH / STORE: none
  std::array<std::optional<Src>, 2> uses{};
};
struct LoadParam { Operand def; std::uint32_t paramIndex; };  // emitter resolves the FP offset
template <typename AssignFn>
auto assignOp(IROp op, AssignFn &&assign) -> IROp {          // no per-opcode special cases
  for (auto &u : op.uses)
    if (u) if (auto *r = std::get_if<Operand>(&*u); r && r->isVirtual)
      *r = {assign(r->val, STORE_AND_LOAD), false};
  if (op.def && op.def->isVirtual) op.def = Operand{assign(op.def->val, STORE_ONLY), false};
  return op;
}
```
Migration: (1) add `LoadParam` and delete the allocator patch loop (this fixes B8), (2) add
def/uses alongside the old fields and switch the allocator to them, (3) switch the emitter,
(4) remove `destination`/`source1`/`source2`/`s2type`.

**D3: parser diagnostics through `std::expected`**
```cpp
struct Diagnostic { int line; std::size_t column; std::string message; };
template <typename T> using ParseResult = std::expected<T, Diagnostic>;
#define FANTA_TRY(var, expr) auto var##Res = (expr); \
  if (!var##Res) return std::unexpected(var##Res.error()); auto var = *var##Res
#define FANTA_CHECK(expr) if (auto res_ = (expr); !res_) return std::unexpected(res_.error())

auto walkParams() -> ParseResult<std::vector<NodeIndex>> {
  std::vector<NodeIndex> params;
  FANTA_CHECK(expect(TokenType::OpenParam, "'('"));
  while (cur().t != TokenType::CloseParam && cur().t != TokenType::Eof) {   // Eof check
    FANTA_TRY(name, expect(TokenType::Identifier, "parameter name"));
    FANTA_CHECK(expect(TokenType::Colon, "':'"));
    FANTA_TRY(type, expect(TokenType::Type, "parameter type"));
    params.push_back(/* FunctionParamDef{name.lexeme, type.lexeme} */ params.size());
    if (cur().t != TokenType::CloseParam) { FANTA_CHECK(expect(TokenType::Comma, "',' or ')'")); }
  }
  FANTA_CHECK(expect(TokenType::CloseParam, "')'"));
  return params;
}
```
Four error styles coexist today: `expected` in the lexer, exceptions in the linker, `-1` sentinels
and `catch(...)` in the assembler, and an empty `ParserError{}`. Token `line` is always 0.

**D4: SimpleIRPass tables and an EmitContext**
```cpp
struct EmitContext { const Parser &p; IRListing &ir; const GlobalTable &gt; LocalTable &lt; };
struct CompareBranches { TokenType tok; std::uint8_t onTrue; std::uint8_t onFalse; };
inline constexpr std::array kCompareBranches{
    CompareBranches{TokenType::Greater,   Op::BEC, Op::BNC},
    CompareBranches{TokenType::Lesser,    Op::BMI, Op::BPL},
    CompareBranches{TokenType::GreaterEq, Op::BPL, Op::BMI},
    CompareBranches{TokenType::LesserEq,  Op::BNC, Op::BEC},
    CompareBranches{TokenType::EqualComp, Op::BEQ, Op::BNE},
    CompareBranches{TokenType::NotEq,     Op::BNE, Op::BEQ}};   // replaces generatePass/FailJump
constexpr auto lowerBinOp(TokenType t) -> std::optional<BinOpLowering>;  // nullopt → diagnostic, not HALT
```
Also: the FunctionCall lowering is duplicated in `emitExpression` and `emitCall`;
`generateAnd`/`generateOr` are dead code; drop the `[](const auto&){}` catch-alls so that a new
AST node without a lowering is a compile error.

**D5: explicit stage types**
```cpp
struct Ast { std::vector<AST::AstNode> nodes; std::vector<AST::NodeIndex> roots;
             auto at(AST::NodeIndex i) const -> const AST::AstNode & { return nodes[i]; } };
struct VirtualIR  { std::vector<FunctionIR> functions; };
struct PhysicalIR { std::vector<FunctionIR> functions; };
template <typename T> concept RegAlloc = requires(T t, const VirtualIR &ir) { { t.allocate(ir) } -> std::same_as<PhysicalIR>; };
inline auto collectGlobals(const Ast &) -> GlobalTable;   // the single owner of global layout
```
Migration: move `common/instructions_impl.hpp` to `vm/` (it includes `vm/cpu.hpp`) → have Parser
expose `const Ast&` → replace `Codegen` (only `extractGlobalNames` is implemented; global offsets
are computed twice) with `collectGlobals` → split the IR types → wire `compile()`. The compiler
also includes `../tui/disasm.hpp`.

**D6: one encoding layout**
```cpp
namespace Encoding {
// [31:26] opcode | [25:21] rd | [20:16] rs1 | [15:11] rs2 or [15:0] imm16 | [25:0] target26
struct Field { unsigned shift; unsigned width;
  constexpr auto mask() const -> std::uint32_t { return (1U << width) - 1U; }
  constexpr auto get(std::uint32_t raw) const -> std::uint32_t { return (raw >> shift) & mask(); }
  constexpr auto put(std::uint32_t v) const -> std::uint32_t { return (v & mask()) << shift; } };
inline constexpr Field OPCODE{26, 6}, RD{21, 5}, RS1{16, 5}, RS2{11, 5}, IMM16{0, 16}, TARGET26{0, 26};
constexpr auto signExtend26(std::uint32_t raw) -> std::int32_t {
  return static_cast<std::int32_t>(TARGET26.get(raw) << 6U) >> 6; }
}
static_assert(Encoding::OPCODE.put(0x16) == (0x16U << 26));
// macro template header: std::int16_t Imm (was uint16_t) — fixes B18 without touching emit()
```
Most of the 92 `bugprone-signed-bitwise` findings are these hand-written shifts.

**D7: table-driven lexer operators**
```cpp
struct Spelling { std::string_view text; TokenType type; };
inline constexpr std::array kOperators{   // two-char spellings first => longest match
    Spelling{"->", TokenType::Arrow}, Spelling{"==", TokenType::EqualComp}, Spelling{"&&", TokenType::And},
    Spelling{"||", TokenType::Or}, /* ... */ Spelling{"-", TokenType::Minus}, Spelling{"+", TokenType::Plus}};
constexpr auto matchOperator(std::string_view rest) -> std::optional<Spelling> {
  for (const auto &op : kOperators) if (rest.starts_with(op.text)) return op;
  return std::nullopt;
}
static_assert(!matchOperator("&x").has_value());   // today this lexes as Mult
```
Also: `(c != ' ' || c != '\0')` is always true (lexer.hpp:87); `using namespace std::literals;`
leaks out of the header; counting lines in a single `advance()` gives tokens their line numbers.
Adding `-Wimplicit-fallthrough` would have caught B5.

**D8: typed assembler operands**
```cpp
struct RegOperand { std::uint8_t idx; }; struct ImmOperand { std::int32_t value; };
struct LabelOperand { std::string_view name; };
using AsmOperand = std::variant<RegOperand, ImmOperand, LabelOperand>;
inline auto parseOperand(std::string_view tok) -> std::expected<AsmOperand, AsmError>;
// "SP" → RegOperand{SP_INDEX}; R16+ → "not addressable"; > 0xFFFF → "build it with LSH/ADD"
```
Also: `split_inst` returns views into a member buffer that the next call overwrites; `::Assembler`
has the same name as `Instructions::Assembler`; `_LITERAL` is a reserved identifier and unused.
The TUI live-patcher would get these better error messages too.

**D9: CPU memory map and flags**
```cpp
namespace MemoryMap {
inline constexpr std::uint32_t RAM_SIZE = 32U * 1024U * 1024U, VRAM_BASE = 0x800000;
inline constexpr std::uint32_t STACK_TOP = VRAM_BASE - 4;   // word-aligned, below VRAM (fixes B2)
}
struct Flags {
  enum FLAG : std::uint8_t { ZERO, NEGATIVE, OVFL, CARRY, /* IE, IP, */ COUNT };
  std::array<bool, COUNT> bits{};
  constexpr auto test(FLAG f, bool inverted) const -> bool { return bits[f] != inverted; }
  constexpr auto setZN(std::uint32_t v) -> void { bits[ZERO] = v == 0; bits[NEGATIVE] = static_cast<std::int32_t>(v) < 0; }
};
```
SP is hard-coded as `registers[16]` in 5 places even though `Info::Registers::SP` exists. The RAM
comment says 16 MB but the code allocates 32 MB. `CYCLES_PER_FRAME` and the `50000` in main.cpp
are two copies of one constant. The interrupts proposal's IE/IP flags become two enumerators.

**D10: constexpr template assembler.** Each instruction format needs a partial-specialisation
macro, and the only consumer is the `line.hpp` demo (hand-counted `Target<-36>`, no labels).
Either drive it from `FANTA_ISA` (`#define FANTA_TEMPLATE_THREE_OP(cls, mn, r, i)
THREE_OP_INST(cls, mn, r, i)` …) or move the demo to a `.asm` file. The `emit()` bodies are
yours, so this is your call.

## 5. Tests

Measured coverage (clang source-based, all 7 ctest targets):

| File | Lines | Missing |
|---|---|---|
| vm/cpu.cpp | 76% | VBLANK counter; LSH-imm/XOR/OR/AND/BEQ/BEC/BNC/JREL dispatch |
| vm/cpu.hpp | 91% | boundary flags, OVFL, stack/VRAM |
| vm/string_assembler.hpp | 85% | error paths, imm bounds, R16+, SP, undefined labels |
| compiler/lexer.hpp | 95% | lone `&`/`\|`, error tokens |
| compiler/parser.cpp | 52% | precedence, for/if shape, every error path (half the gap is printAST) |
| compiler/SimpleIRPass.cpp | 81% | calls partly, globals beyond one slot, Mult/Slash, missing for-init |
| compiler/allocator.cpp | 95% | spills never *executed*, >14 params |
| compiler/instruction_emit.cpp | 81% | link errors, >0xFFFF immediates |
| vm/main.cpp, compiler_main.cpp, vm/line.hpp | untested | not linked into any test |

### 5.1 Tests added (all pass; `ctest`: 7/7 targets, 98 test cases)

- `test/cpu.cpp`: Flags – ADD Boundary Values; Flags – SUB Boundary Values; Immediates – 16-bit
  Payload Is Zero-Extended; Signed Branches – BMI/BPL After CMP; PC-Relative CALL/RET; PUSH/POP
  Round Trip Restores SP; Load/Store With Max 16-bit Offset.
- `test/codegen.cpp`: Call – Two Args Returned Through R0; Call – Recursion Preserves Callee-Saved
  Registers; Call – Stack Pointer Balanced After Program; Spills – 17 Live Locals Survive Register
  Pressure; Negative Values Compare Signed; Two Globals Have Independent Slots; Unknown Function
  Fails At Link Time; Missing main Fails At Link Time.
- `test/ops.cpp` (+ `#include "../common/cpu_info.hpp"`): Registry Round Trip – Every Opcode
  Encodes At Bit 26; Registry Matches cpu_info Opcode Constants; Unknown Mnemonic Throws From
  Registry.
- `test/string_assembler.cpp`: Lowercase Mnemonics And Registers Are Normalised; Literals Are Hex By
  Default; Immediate Bounds – 0x0000 and 0xFFFF Accepted; Unknown Mnemonic And Missing Operands
  Fail Cleanly; Negative Branch Offset Round Trips Through Decode.
- `test/lexer.cpp`: Comparison And Logical Operator Lexing; Unknown Symbol Reports UnknownToken And
  Advances; Digits Followed By Letters Is MalformedToken.
- `test/parser.cpp`: Precedence – Mult Binds Tighter Than Plus; Precedence – Minus Is Left
  Associative; Precedence – Comparison Binds Tighter Than && / ||; For Loop AST Shape.

Note: the existing `ops_test` round-trip only checks the template path. The `CALL`/`JREL` bug (B1)
is in the *runtime table* format field, which no test checks yet. Add a test like T-B1 below once
you've decided how to fix it.

### 5.2 Failing tests (not applied). Add each one with its fix.

```cpp
// T1 (B2) — test/cpu.cpp
TEST_CASE("Stack Push Must Not Touch VRAM") {
  CPU cpu{};
  cpu.push_stack(0xAABBCCDD);
  auto *vram = cpu.get_vram();
  REQUIRE_SAME(0, vram[0]); REQUIRE_SAME(0, vram[1]); REQUIRE_SAME(0, vram[2]);
  REQUIRE_SAME(0u, cpu.get_sp() % 4);
}

// T2 (B3), T3 (B4), T4 (B8), T5 (B9), T11 (B10) — test/codegen.cpp
TEST_CASE("Literal Above 0xFFFF Is Materialised Fully") {
  REQUIRE_SAME(70000, compileAndRun("fn main() -> int { return 70000; }").registers[0]);
}
TEST_CASE("Multiplication Is Compiled (Or Rejected), Not Emitted As HALT") {
  std::string code = "fn main() -> int { let x: int = 2 * 3; return x + 1; }";
  REQUIRE_SAME(7, compileAndRun(code).registers[0]);   // or assert a compile error, per your fix
}
TEST_CASE("Fifteen Params - Last Param Load Gets Frame Adjustment") {
  std::string params, args;
  for (int i = 1; i <= 15; i++) {
    params += (i > 1 ? ", p" : "p") + std::to_string(i) + ": int";
    args += (i > 1 ? ", " : "") + std::to_string(i);
  }
  std::string code = "fn f(" + params + ") -> int { return p15; }"
                     "fn main() -> int { return f(" + args + "); }";
  REQUIRE_SAME(15, compileAndRun(code).registers[0]);
}
TEST_CASE("Params After A long Arg Are Read From The Right Slot") {
  std::string code = "fn f(a: long, b: int) -> int { return b; }"
                     "fn main() -> int { return f(1, 2); }";
  REQUIRE_SAME(2, compileAndRun(code).registers[0]);
}
TEST_CASE("For Loop Without Init Clause") {
  std::string code = "fn main() -> int { let i: int = 0;"
                     "for (; i < 3; i = i + 1) { } return i; }";
  int got = -1;
  try { got = compileAndRun(code).registers[0]; }
  catch (const std::bad_variant_access &) { got = -2; }
  REQUIRE_SAME(3, got);
}

// T6 (B6) — test/parser.cpp. These hang today, so add
// set_tests_properties(parser_test PROPERTIES TIMEOUT 10) in CMakeLists.txt first.
TEST_CASE("Statement After Return Terminates") {
  Parser p{"fn main() -> int { return 1; x; }"}; p.fullWalk();
  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::FunctionDef>(p.getCurrentRoot().t));
}
TEST_CASE("Unterminated Body Terminates") { Parser p{"fn main() -> int { return 1;"}; p.fullWalk(); REQUIRE_TRUE(true); }
TEST_CASE("Malformed Param List Terminates") { Parser p{"fn main(1) -> int { return 1; }"}; p.fullWalk(); REQUIRE_TRUE(true); }

// T12 (B11) — test/parser.cpp
TEST_CASE("Unary Minus Literal Does Not Produce A Dangling Node") {
  Parser p{"let x : int = -1;"}; p.walk();
  auto decl = std::get<Fanta::AST::VariableDecl>(p.getCurrentRoot().t);
  auto node = p.getNodeAtIndex(decl.defineNode);
  if (std::holds_alternative<Fanta::AST::BinaryOperator>(node.t))
    REQUIRE_TRUE(std::get<Fanta::AST::BinaryOperator>(node.t).lhsOp != static_cast<Fanta::AST::NodeIndex>(-1));
  else
    REQUIRE_SAME(-1, std::get<Fanta::AST::IntLiteral>(node.t).literal);
}

// T13 (B5) — test/lexer.cpp
TEST_CASE("Lone & And | Are Not Multiplication") {
  std::string src = "a & b"; Lexer lx{src}; lx.getToken();
  auto amp = lx.getToken();
  REQUIRE_TRUE(!amp.has_value() || amp->t != Lexer::TokenType::Mult);
  std::string src2 = "|"; Lexer lx2{src2};
  auto bar = lx2.getToken();
  REQUIRE_TRUE(!bar.has_value() || bar->t != Lexer::TokenType::Mult);
}

// T7 (B12), T8 (B14), T14 (B13) — test/cpu.cpp
TEST_CASE("Signed Compare - INT32_MIN < 1 Must Read As Negative") {
  using namespace Instructions;
  constexpr auto code = Program<Cmp<Reg<0>, Reg<1>>, Halt>::load();
  CPU cpu{}; cpu.load_rom(code);
  cpu.registers[0] = 0x80000000; cpu.registers[1] = 1;
  cpu.run_cycle();
  REQUIRE_TRUE(!cpu.is_carry_set());
  REQUIRE_TRUE(cpu.is_neg_set() != cpu.is_overflow_set());   // N^V == signed less-than
}
TEST_CASE("VBLANK Counter Is Per-CPU") {
  CPU warm{};
  while (warm.cip_interrupts[0] == 0) { warm.set_pc(0); warm.run_cycle(); }
  for (int i = 0; i < 49990; i++) { warm.set_pc(0); warm.run_cycle(); }
  CPU fresh{};
  for (int i = 0; i < 20; i++) { fresh.set_pc(0); fresh.run_cycle(); }
  REQUIRE_SAME(0, fresh.cip_interrupts[0]);
}
TEST_CASE("LSH By 32 Or More Yields Zero") {
  using namespace Instructions;
  constexpr auto code = Program<Mov<Reg<0>, Literal<1>>, Lsh<Reg<0>, Reg<0>, Literal<32>>, Halt>::load();
  CPU cpu{}; cpu.load_rom(code); cpu.run_until_halt();
  REQUIRE_SAME(0u, cpu.registers[0]);
  REQUIRE_TRUE(cpu.is_zero_set());
}

// T9 (B15), T10 (B16) — test/string_assembler.cpp
TEST_CASE("Immediate Bounds - 0x10000 Rejected") {
  Assembler a{};
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("MOV R1, #10000", 0));
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("ADD R1, R1, #10000", 0));
}
TEST_CASE("Direct R16+ Register Access Is Rejected") {
  Assembler a{};
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("ADD R1, R2, R16", 0));
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("MOV R20, R1", 0));
}
TEST_CASE("Undefined Label Is An Assembly Error") {
  Assembler a{};
  a.scan_for_labels("START:\nBNE NOWHERE");
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("BNE NOWHERE", 4));
  REQUIRE_TRUE(!a.does_label_exist("NOWHERE"));
}
TEST_CASE("SP As A Source Operand Is A Register, Not An Immediate") {
  Assembler a{};
  REQUIRE_SAME(Instructions::parse_two(0x3, 1, 16, false), a.assemble("MOV R1, SP", 0));
  REQUIRE_SAME(Instructions::parse_three(0x1, 1, 2, 16, false), a.assemble("ADD R1, R2, SP", 0));
}

// T-B1 (B1) — test/ops.cpp, or as a static_assert in common/instructions.hpp
TEST_CASE("PC-Relative Instructions Are Registered As BRANCH") {
  using namespace Instructions;
  REQUIRE_SAME(InstFormat::BRANCH, Registry::fetch("CALL").fmt);
  REQUIRE_SAME(InstFormat::BRANCH, Registry::fetch("JREL").fmt);
}
```

## Decisions (same day)

- Identifiers **should** allow `_`, so the lexer is wrong. User is fixing it.
- `set_tests_properties(parser_test PROPERTIES TIMEOUT 10)` added to `CMakeLists.txt`. The T6
  hang tests can now be added alongside the B6 fix.
- `.clang-tidy`: disabled `modernize-use-trailing-return-type` and
  `modernize-use-designated-initializers`. `-Wimplicit-fallthrough` added to the build: it
  reports exactly B5 (lexer.hpp:207, 216) and nothing else.
- `docs/interrupts_proposal.md` rewritten. EI/DI/RETI are now `0x21`/`0x22`/`0x23`. It
  describes the current CIP model, depends on B2/B14, and lists open questions.
  `docs/cooperative_interrupts.md` is also out of date: it describes CIP as a PC-relative
  `BRANCH` with an IP bit in `status_reg`, but the implementation takes a line number and uses
  `cip_interrupts[]`.

## 6. Previous findings

None: this is the first review.

## Workflow notes

- Each reviewer builds in its own scratch directory, so `build/` stays clean.
- Suggested `.clang-tidy` tuning to discuss: `modernize-use-trailing-return-type` (79) and
  `modernize-use-designated-initializers` (183) are mostly noise for this codebase. Consider
  enabling `-Wimplicit-fallthrough`, which would have caught B5.

## Follow-up: file moves (2026-10-17)

`common/instructions_impl.hpp` (D5) is now `vm/execute.hpp`, and its types live in `namespace Exec`.
`parse_as_signed` is now `inline`. Paths in this report still use the old name; line numbers
match apart from a 5-line shift from the namespace header.
