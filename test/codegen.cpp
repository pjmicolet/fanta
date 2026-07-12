#include "../tui/disasm.hpp"
#include <codegen.hpp>
#include <testframework/testing.hpp>

TEST_CASE("Global Namespace") {
  std::string code = "let a : int = 123;"
                     "fn main(b:int, c:float) -> int {"
                     "let d : int = b + c;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::Codegen cg{};

  cg.extractGlobalNames(p);

  REQUIRE_SAME(cg.globalNameSpace.size(), 2);

  REQUIRE_TRUE(std::holds_alternative<Fanta::GlobalFuncInfo>(
      cg.globalNameSpace["main"]));
  REQUIRE_TRUE(
      std::holds_alternative<Fanta::GlobalVarInfo>(cg.globalNameSpace["a"]));

  const auto &func =
      std::get<Fanta::GlobalFuncInfo>(cg.globalNameSpace["main"]);

  REQUIRE_SAME(func.paramTypes.size(), 2);
  REQUIRE_SAME(func.paramTypes[0], "int");
  REQUIRE_SAME(func.paramTypes[1], "float");
}

#include <SimpleIRPass.hpp>
#include <allocator.hpp>
#include <cpu.hpp>
#include <instruction_emit.hpp>

TEST_CASE("Instruction Emitter & Execution Test") {
  std::string code = "fn main(b: int, c: int) -> int {"
                     "let d : int = b + c;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;
  auto virtualIR = pass.outputIR(p, gt);

  Fanta::Allocator alloc{};
  Fanta::RegAllocIR rir;
  for (const auto &func : virtualIR.functions) {
    rir.functions.push_back(alloc.assignFunc(func));
  }

  Fanta::InstructionEmitter emitter{};
  auto insts = emitter.outputInstructions(rir, gt);

  std::println("=== Compiled Program Disassembly ===");
  std::print("{}", Fanta::disassembleProgram(insts));

  REQUIRE_TRUE(insts.size() > 0);

  CPU cpu{};
  for (size_t i = 0; i < insts.size(); i++) {
    cpu.store(i * 4, insts[i]);
  }

  uint32_t sp = cpu.registers[16]; // 0x7FFFFF

  // Store parameter 2 (c = 20) at 0x7FFFFF
  cpu.store(sp, 20);

  // Store parameter 1 (b = 10) at 0x7FFFFB
  sp -= 4;
  cpu.store(sp, 10);

  // Set SP to 0x7FFFF7 (the next free slot where R15 will be pushed)
  cpu.registers[16] = sp - 4;

  // Run the CPU for 14 cycles to stop right before epilogue pops registers
  // (CALL to main, then PUSH FP/R1-R5, SUB SP, MOV FP, LOAD R1, LOAD R2,
  // MOV R3, MOV R4, ADD R5 = 14 instructions before the epilogue begins).
  for (int i = 0; i < 14; i++) {
    cpu.run_cycle();
  }

  // Under the physical register assignments for this function,
  // b will be loaded into R0, c into R1, and d (the result of b + c) will be
  // computed into R5. So R5 should contain 30 before epilogue pops.
  REQUIRE_SAME(30, cpu.registers[5]);
}

namespace {
auto compileAndRun(const std::string &code) -> CPU {
  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;
  auto virtualIR = pass.outputIR(p, gt);

  Fanta::Allocator alloc{};
  Fanta::RegAllocIR rir;
  for (const auto &func : virtualIR.functions) {
    rir.functions.push_back(alloc.assignFunc(func));
  }

  Fanta::InstructionEmitter emitter{};
  auto insts = emitter.outputInstructions(rir, gt);

  CPU cpu{};
  for (size_t i = 0; i < insts.size(); i++) {
    cpu.store(i * 4, insts[i]);
  }
  cpu.run_until_halt();
  return cpu;
}

// Builds: fn main() -> int { let a: int = A; let b: int = B; if (a OP b) {
// return 1; } return 0; }
auto ifComparisonProgram(const std::string &op, int a, int b) -> std::string {
  return "fn main() -> int {"
         "let a: int = " +
         std::to_string(a) +
         ";"
         "let b: int = " +
         std::to_string(b) +
         ";"
         "if (a " +
         op +
         " b) {"
         "return 1;"
         "}"
         "return 0;"
         "}";
}

// Builds: fn main() -> int { let a=A; let b=B; let c=C; let d=D;
// if (COND) { return 1; } return 0; }
auto ifLogicalProgram(const std::string &cond, int a, int b, int c, int d)
    -> std::string {
  return "fn main() -> int {"
         "let a: int = " +
         std::to_string(a) +
         ";"
         "let b: int = " +
         std::to_string(b) +
         ";"
         "let c: int = " +
         std::to_string(c) +
         ";"
         "let d: int = " +
         std::to_string(d) +
         ";"
         "if (" +
         cond +
         ") {"
         "return 1;"
         "}"
         "return 0;"
         "}";
}
} // namespace

TEST_CASE("If Statement - Greater Than") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram(">", 5, 3)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram(">", 3, 5)).registers[0]);
}

TEST_CASE("If Statement - Less Than") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram("<", 3, 5)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram("<", 5, 3)).registers[0]);
}

TEST_CASE("If Statement - Greater Or Equal") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram(">=", 5, 5)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram(">=", 3, 5)).registers[0]);
}

TEST_CASE("If Statement - Less Or Equal") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram("<=", 5, 5)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram("<=", 5, 3)).registers[0]);
}

TEST_CASE("If Statement - Equal") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram("==", 4, 4)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram("==", 4, 5)).registers[0]);
}

TEST_CASE("If Statement - Not Equal") {
  REQUIRE_SAME(1, compileAndRun(ifComparisonProgram("!=", 4, 5)).registers[0]);
  REQUIRE_SAME(0, compileAndRun(ifComparisonProgram("!=", 4, 4)).registers[0]);
}

TEST_CASE("If/Else - Else Body Actually Executes When Condition Is False") {
  std::string code = "fn main() -> int {"
                     "let a: int = 3;"
                     "let b: int = 5;"
                     "if (a > b) {"
                     "let x: int = 99;"
                     "} else {"
                     "return 2;"
                     "}"
                     "return 1;"
                     "}";
  REQUIRE_SAME(2, compileAndRun(code).registers[0]);
}

TEST_CASE("If/Else - True Branch Falls Through Past Else When Body Doesn't "
          "Return") {
  // Regression test: the if-body ("let x: int = 99;") doesn't return, so
  // execution must jump past the else body ("return 2;") to the trailing
  // "return 1;" rather than falling through into the else body.
  std::string code = "fn main() -> int {"
                     "let a: int = 5;"
                     "let b: int = 3;"
                     "if (a > b) {"
                     "let x: int = 99;"
                     "} else {"
                     "return 2;"
                     "}"
                     "return 1;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("If/Else - Nested If With Return Doesn't Confuse Outer Else") {
  // Regression test: a Return inside a nested if (taken or not) must not
  // make the outer if think its own body ends in a return. Here the outer
  // if-body's real tail ("let x: int = 7;") doesn't return, so the outer
  // else ("return 2;") must still be skipped in favor of the trailing
  // "return 1;".
  std::string code = "fn main() -> int {"
                     "let a: int = 5;"
                     "let b: int = 3;"
                     "let c: int = 1;"
                     "let d: int = 10;"
                     "if (a > b) {"
                     "if (c > d) {"
                     "return 99;"
                     "}"
                     "let x: int = 7;"
                     "} else {"
                     "return 2;"
                     "}"
                     "return 1;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

// --- Logical && / || ---

TEST_CASE("Logical And - Both Sides True") {
  REQUIRE_SAME(1, compileAndRun(ifLogicalProgram("a > b && c > d", 5, 3, 10, 1))
                      .registers[0]);
}

TEST_CASE("Logical And - Left True Right False") {
  REQUIRE_SAME(0, compileAndRun(ifLogicalProgram("a > b && c > d", 5, 3, 1, 10))
                      .registers[0]);
}

TEST_CASE("Logical And - Left False Right True (Short-Circuits)") {
  // c > d is never actually true here relative to the left side failing
  // first; this confirms the left side alone is enough to fail without the
  // right side needing to be false too.
  REQUIRE_SAME(0, compileAndRun(ifLogicalProgram("a > b && c > d", 3, 5, 10, 1))
                      .registers[0]);
}

TEST_CASE("Logical And - Both Sides False") {
  REQUIRE_SAME(0, compileAndRun(ifLogicalProgram("a > b && c > d", 3, 5, 1, 10))
                      .registers[0]);
}

TEST_CASE("Logical And - Mixed Comparators True") {
  REQUIRE_SAME(1,
               compileAndRun(ifLogicalProgram("a == 5 && d <= c", 5, 3, 10, 1))
                   .registers[0]);
}

TEST_CASE("Logical And - Mixed Comparators False") {
  REQUIRE_SAME(0,
               compileAndRun(ifLogicalProgram("a == 5 && d >= c", 5, 3, 10, 1))
                   .registers[0]);
}

TEST_CASE("Logical And - Combined With Else, Condition False") {
  std::string code = "fn main() -> int {"
                     "let a: int = 5; let b: int = 3;"
                     "let c: int = 1; let d: int = 10;"
                     "if (a > b && c > d) {"
                     "return 1;"
                     "} else {"
                     "return 2;"
                     "}"
                     "}";
  REQUIRE_SAME(2, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical And - Combined With Else, Condition True") {
  std::string code = "fn main() -> int {"
                     "let a: int = 5; let b: int = 3;"
                     "let c: int = 10; let d: int = 1;"
                     "if (a > b && c > d) {"
                     "return 1;"
                     "} else {"
                     "return 2;"
                     "}"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Or - Both Sides True") {
  REQUIRE_SAME(1, compileAndRun(ifLogicalProgram("a > b || c > d", 5, 3, 10, 1))
                      .registers[0]);
}

TEST_CASE("Logical Or - Left True Right False (Short-Circuits)") {
  REQUIRE_SAME(1, compileAndRun(ifLogicalProgram("a > b || c > d", 5, 3, 1, 10))
                      .registers[0]);
}

TEST_CASE("Logical Or - Left False Right True") {
  REQUIRE_SAME(1, compileAndRun(ifLogicalProgram("a > b || c > d", 3, 5, 10, 1))
                      .registers[0]);
}

TEST_CASE("Logical Or - Both Sides False") {
  REQUIRE_SAME(0, compileAndRun(ifLogicalProgram("a > b || c > d", 3, 5, 1, 10))
                      .registers[0]);
}

// --- Nested logical operators: Or nested inside And's left operand ---

TEST_CASE("Logical Nested - Or-Inside-And, Or Settled True By Left Side") {
  std::string code = "fn main() -> int {"
                     "let a: int = 5; let b: int = 3;"
                     "let c: int = 1; let d: int = 10;"
                     "let e: int = 5; let f: int = 3;"
                     "if ((a > b || c > d) && e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - Or-Inside-And, Or False") {
  std::string code = "fn main() -> int {"
                     "let a: int = 3; let b: int = 5;"
                     "let c: int = 1; let d: int = 10;"
                     "let e: int = 5; let f: int = 3;"
                     "if ((a > b || c > d) && e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(0, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - Or-Inside-And, Or True But Right Side False") {
  std::string code = "fn main() -> int {"
                     "let a: int = 5; let b: int = 3;"
                     "let c: int = 1; let d: int = 10;"
                     "let e: int = 3; let f: int = 5;"
                     "if ((a > b || c > d) && e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(0, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - Or-Inside-And, Or Settled True By Right Side") {
  std::string code = "fn main() -> int {"
                     "let a: int = 3; let b: int = 5;"
                     "let c: int = 10; let d: int = 1;"
                     "let e: int = 5; let f: int = 3;"
                     "if ((a > b || c > d) && e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - And-Inside-Or, RHS OR is Right") {
  std::string code = "fn main() -> int {"
                     "let a: int = 5; let b: int = 3;"
                     "let c: int = 1; let d: int = 10;"
                     "let e: int = 5; let f: int = 3;"
                     "if ((a > b && c > d) || e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - And-Inside-Or, LHS OR is Right") {
  std::string code = "fn main() -> int {"
                     "let a: int = 15; let b: int = 3;"
                     "let c: int = 11; let d: int = 10;"
                     "let e: int = 1; let f: int = 3;"
                     "if ((a > b && c > d) || e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(1, compileAndRun(code).registers[0]);
}

TEST_CASE("Logical Nested - And-Inside-Or, All Wrong") {
  std::string code = "fn main() -> int {"
                     "let a: int = 1; let b: int = 3;"
                     "let c: int = 1; let d: int = 10;"
                     "let e: int = 1; let f: int = 3;"
                     "if ((a > b && c > d) || e > f) { return 1; }"
                     "return 0;"
                     "}";
  REQUIRE_SAME(0, compileAndRun(code).registers[0]);
}

// --- Assignment (emitBinaryOpIR) ---

TEST_CASE("Assignment - Local Variable") {
  std::string code = "fn main() -> int {"
                     "let x: int = 5;"
                     "x = 10;"
                     "return x;"
                     "}";
  REQUIRE_SAME(10, compileAndRun(code).registers[0]);
}

TEST_CASE("Assignment - Local Variable Reassigned From Expression") {
  std::string code = "fn main() -> int {"
                     "let x: int = 5;"
                     "let y: int = 3;"
                     "x = y + 20;"
                     "return x;"
                     "}";
  REQUIRE_SAME(23, compileAndRun(code).registers[0]);
}

TEST_CASE("Assignment - Global Variable Write Then Read") {
  std::string code = "let g: int = 1;"
                     "fn main() -> int {"
                     "g = 42;"
                     "return g;"
                     "}";
  REQUIRE_SAME(42, compileAndRun(code).registers[0]);
}

TEST_CASE("Global Variable Initializer Takes Effect") {
  // Regression test: emitGlobalVariableIR's STORE used isVirtual=false for
  // its base-address register, so the allocator never assigned it a real
  // physical register and the initial value landed at the wrong address.
  std::string code = "let g: int = 7;"
                     "fn main() -> int {"
                     "return g;"
                     "}";
  REQUIRE_SAME(7, compileAndRun(code).registers[0]);
}

// --- for loops ---

TEST_CASE("For Loop - Sums 0 Through 4") {
  std::string code = "fn main() -> int {"
                     "let sum: int = 0;"
                     "for (let i: int = 0; i < 5; i = i + 1) {"
                     "sum = sum + i;"
                     "}"
                     "return sum;"
                     "}";
  REQUIRE_SAME(10, compileAndRun(code).registers[0]);
}

TEST_CASE("For Loop - Condition False Up Front Never Runs Body") {
  std::string code = "fn main() -> int {"
                     "let sum: int = 99;"
                     "for (let i: int = 0; i < 0; i = i + 1) {"
                     "sum = 1;"
                     "}"
                     "return sum;"
                     "}";
  REQUIRE_SAME(99, compileAndRun(code).registers[0]);
}

TEST_CASE("For Loop - Nested If Inside Body") {
  std::string code = "fn main() -> int {"
                     "let count: int = 0;"
                     "for (let i: int = 0; i < 10; i = i + 1) {"
                     "if (i > 4) {"
                     "count = count + 1;"
                     "}"
                     "}"
                     "return count;"
                     "}";
  REQUIRE_SAME(5, compileAndRun(code).registers[0]);
}
