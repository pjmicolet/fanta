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

  // Run the CPU for 17 cycles to stop right before epilogue pops registers
  for (int i = 0; i < 17; i++) {
    cpu.run_cycle();
  }

  // Under the physical register assignments for this function,
  // b will be loaded into R0, c into R1, and d (the result of b + c) will be
  // computed into R4. So R4 should contain 30 before epilogue pops.
  REQUIRE_SAME(30, cpu.registers[4]);
}
