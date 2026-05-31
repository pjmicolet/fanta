#include <testframework/testing.hpp>
#include <parser.hpp>

TEST_CASE("Basic Assignment") {
  Parser p{"let hello : int = 12"};
  p.walk();
  auto node = p.getCurrentRoot();

  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(node.t));

  auto varDeclNode = std::get<Fanta::AST::VariableDecl>(node.t);

  REQUIRE_SAME("hello", varDeclNode.name); 
  REQUIRE_SAME("int", varDeclNode.type); 

  auto rhsVal = std::get<Fanta::AST::IntLiteral>(p.getNodeAtIndex(varDeclNode.defineNode).t);

  REQUIRE_SAME(12, rhsVal.literal);
}
