#include <testframework/testing.hpp>
#include <parser.hpp>

TEST_CASE("Basic Assignment") {
  Parser p{"let hello : int = 12;"};
  p.walk();
  auto node = p.getCurrentRoot();

  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(node.t));

  auto varDeclNode = std::get<Fanta::AST::VariableDecl>(node.t);

  REQUIRE_SAME("hello", varDeclNode.name); 
  REQUIRE_SAME("int", varDeclNode.type); 

  auto rhsVal = std::get<Fanta::AST::IntLiteral>(p.getNodeAtIndex(varDeclNode.defineNode).t);

  REQUIRE_SAME(12, rhsVal.literal);
}

TEST_CASE("Basic Assignment Expression") {
  Parser pExpr{"let hello : int = a + 12 - 45 * c;"};
  pExpr.walk();
  auto exprNode = pExpr.getCurrentRoot();
  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(exprNode.t));
  auto varDeclNode = std::get<Fanta::AST::VariableDecl>(exprNode.t);
  auto rhsVal = std::get<Fanta::AST::BinaryOperator>(pExpr.getNodeAtIndex(varDeclNode.defineNode).t);

  REQUIRE_SAME(Lexer::TokenType::Minus, rhsVal.type);
}

TEST_CASE("Function Definition") {
  Parser pFn{"fn add(a: int, b: int) -> int { let c: int = a + b; }"};
  pFn.walk();
  
  auto node = pFn.getCurrentRoot();
  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::FunctionDef>(node.t));
  
  auto func = std::get<Fanta::AST::FunctionDef>(node.t);
  REQUIRE_SAME("add", func.name);
  REQUIRE_SAME("int", func.retType);
  REQUIRE_SAME(2, func.params.size());
}

TEST_CASE("Basic Function Expression") {
  Parser pExpr{"a(1,2,3+3);"};
  pExpr.walk();
  auto exprNode = pExpr.getCurrentRoot();

  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::FunctionCall>(exprNode.t));
}

TEST_CASE("Basic Function Expression") {
  Parser pExpr{"fn hello(a : int, b : int) -> int { \
    let d : int = 10; \
    d = d + 1;\
    hello(d, d);\
  }"}; \
  pExpr.walk();
  auto exprNode = pExpr.getCurrentRoot();

}
