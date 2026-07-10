#include <codegen.hpp>
#include <testframework/testing.hpp>
#include "../tui/disasm.hpp"

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
