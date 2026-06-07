#include <SimpleIRPass.hpp>
#include <testframework/testing.hpp>

TEST_CASE("Function to IR") {
  std::string code = "fn run(b:int, c:float) -> int {"
                     "let d : int = b + c;"
                     "}";

  Parser p{code};
  p.fullWalk();

  Fanta::SimpleIRPass simpleIRPass{};

  Fanta::GlobalTable gt{};

  auto ir = simpleIRPass.outputIR(p, gt);
}
