#include <SimpleIRPass.hpp>
#include <allocator.hpp>
#include <testframework/testing.hpp>

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
  REQUIRE_TRUE(actualIR.functions.size() == 1);

  const auto &actualFunc = actualIR.functions[0];
  REQUIRE_TRUE(actualFunc.insts.size() == 7);
  printIR(actualIR);
}
