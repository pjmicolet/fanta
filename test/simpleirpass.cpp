#include <SimpleIRPass.hpp>
#include <allocator.hpp>
#include <testframework/testing.hpp>
#include <fanta_utils.hpp>

// Verify concept satisfaction

TEST_CASE("SimpleIRPass & Allocator End-to-End Test") {
  std::string code = "fn run(b: int, c: int) -> int {"
                     "let d : int = b + c;"
                     "let e : int = d - 10;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;

  auto actualIR = pass.outputIR(p, gt);
  // outputIR always prepends an __init function ahead of user functions.
  REQUIRE_TRUE(actualIR.functions.size() == 2);

  const auto &actualFunc = actualIR.functions[1];
  REQUIRE_TRUE(actualFunc.insts.size() == 7);
  printIR(actualIR);

  Fanta::Allocator alloc{};
  const auto &allocatedFunc = alloc.assignFunc(actualFunc);
  Fanta::IR dmp{{allocatedFunc}, {}};
  printIR(dmp);
}

TEST_CASE("Register Spilling Under Register Pressure") {
  std::string code = "fn run() -> int {"
                     "let a1: int = 1;"
                     "let a2: int = 2;"
                     "let a3: int = 3;"
                     "let a4: int = 4;"
                     "let a5: int = 5;"
                     "let a6: int = 6;"
                     "let a7: int = 7;"
                     "let a8: int = 8;"
                     "let a9: int = 9;"
                     "let a10: int = 10;"
                     "let a11: int = 11;"
                     "let a12: int = 12;"
                     "let a13: int = 13;"
                     "let a14: int = 14;"
                     "let a15: int = 15;"
                     "let a16: int = 16;"
                     "let a17: int = 17;"
                     "let res: int = a1 + a2;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;

  auto actualIR = pass.outputIR(p, gt);
  // outputIR always prepends an __init function ahead of user functions.
  REQUIRE_TRUE(actualIR.functions.size() == 2);

  std::println("\n--- Spilling Test: Virtual IR ---");
  printIR(actualIR);

  Fanta::Allocator alloc{};
  const auto &allocatedFunc = alloc.assignFunc(actualIR.functions[1]);

  std::println("\n--- Spilling Test: Allocated IR ---");
  Fanta::IR dmp{{allocatedFunc}, {}};
  printIR(dmp);
}


TEST_CASE("Register Allocation - Basic Reuse and Operand Update") {
  std::string code = "fn run(a: int) -> int {"
                     "let b: int = a + 5;"
                     "let c: int = b + a;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;

  auto actualIR = pass.outputIR(p, gt);
  // outputIR always prepends an __init function ahead of user functions.
  REQUIRE_TRUE(actualIR.functions.size() == 2);

  Fanta::Allocator alloc{};
  const auto &allocatedFunc = alloc.assignFunc(actualIR.functions[1]);

  // Check that all operands in instructions have isVirtual == false
  for (const auto &inst : allocatedFunc.insts) {
    std::visit(overloaded{
      [&](const Fanta::IROp &op) {
        REQUIRE_TRUE(!op.destination.isVirtual);
        REQUIRE_TRUE(!op.source1.isVirtual);
        REQUIRE_TRUE(!op.source2.isVirtual);
      },
      [&](const auto &other) {}
    }, inst);
  }

  // Check that calleeRegs has the used physical registers and they are all physical
  REQUIRE_TRUE(allocatedFunc.calleeRegs.size() > 0);
  for (const auto &reg : allocatedFunc.calleeRegs) {
    REQUIRE_TRUE(!reg.isVirtual);
    REQUIRE_TRUE(reg.val < 15);
  }
}

TEST_CASE("Register Spilling - Specific Store and Load Verification") {
  std::string code = "fn run() -> int {"
                     "let a0: int = 0;"
                     "let a1: int = 1;"
                     "let a2: int = 2;"
                     "let a3: int = 3;"
                     "let a4: int = 4;"
                     "let a5: int = 5;"
                     "let a6: int = 6;"
                     "let a7: int = 7;"
                     "let a8: int = 8;"
                     "let a9: int = 9;"
                     "let a10: int = 10;"
                     "let a11: int = 11;"
                     "let a12: int = 12;"
                     "let a13: int = 13;"
                     "let a14: int = 14;"
                     "let a15: int = 15;"
                     "let use: int = a0 + 1;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::GlobalTable gt;
  Fanta::SimpleIRPass pass;

  auto actualIR = pass.outputIR(p, gt);
  // outputIR always prepends an __init function ahead of user functions.
  REQUIRE_TRUE(actualIR.functions.size() == 2);
  Fanta::Allocator alloc{};
  const auto &allocatedFunc = alloc.assignFunc(actualIR.functions[1]);

  // Count the number of Store (0x8) and Load (0x9) instructions
  size_t storeCount = 0;
  size_t loadCount = 0;

  for (const auto &inst : allocatedFunc.insts) {
    std::visit(overloaded{
      [&](const Fanta::IROp &op) {
        if (op.opcode == 0x8) {
          storeCount++;
          // A store instruction should store a register into stack using R15 (FP) as base
          REQUIRE_SAME(op.source1.val, 15);
          REQUIRE_TRUE(!op.source1.isVirtual);
        } else if (op.opcode == 0x9) {
          loadCount++;
          // A load instruction should load from stack using R15 as base
          REQUIRE_SAME(op.source1.val, 15);
          REQUIRE_TRUE(!op.source1.isVirtual);
        }
      },
      [&](const auto &other) {}
    }, inst);
  }

  // There must be at least one store and one load instruction
  REQUIRE_TRUE(storeCount >= 1);
  REQUIRE_TRUE(loadCount >= 1);
}


