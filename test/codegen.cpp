#include <codegen.hpp>
#include <testframework/testing.hpp>

TEST_CASE("Global Namespace") {
  std::string code = 
    "let a : int = 123;"
    "fn run(b:int, c:float) -> int {"
      "let d : int = b + c;"
    "}";

  Parser p{code};
  p.fullWalk();

  Fanta::Codegen cg{};

  cg.extractGlobalNames(p);

  REQUIRE_SAME(cg.globalNameSpace.size(), 2);

  REQUIRE_TRUE(std::holds_alternative<Fanta::GlobalFuncInfo>(cg.globalNameSpace["run"]));
  REQUIRE_TRUE(std::holds_alternative<Fanta::GlobalVarInfo>(cg.globalNameSpace["a"]));

  const auto& func = std::get<Fanta::GlobalFuncInfo>(cg.globalNameSpace["run"]);

  REQUIRE_SAME(func.paramTypes.size(), 2);
  REQUIRE_SAME(func.paramTypes[0], "int");
  REQUIRE_SAME(func.paramTypes[1], "float");
}
