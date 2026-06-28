#include "ast.hpp"
#include "lexer.hpp"
#include <parser.hpp>
#include <testframework/testing.hpp>
#include <variant>

TEST_CASE("Basic Assignment") {
  Parser p{"let hello : int = 12;"};
  p.walk();
  auto node = p.getCurrentRoot();

  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(node.t));

  auto varDeclNode = std::get<Fanta::AST::VariableDecl>(node.t);

  REQUIRE_SAME("hello", varDeclNode.name);
  REQUIRE_SAME("int", varDeclNode.type);

  auto rhsVal = std::get<Fanta::AST::IntLiteral>(
      p.getNodeAtIndex(varDeclNode.defineNode).t);

  REQUIRE_SAME(12, rhsVal.literal);
}

TEST_CASE("Basic Assignment Expression") {
  Parser pExpr{"let hello : int = a + 12 - 45 * c;"};
  pExpr.walk();
  auto exprNode = pExpr.getCurrentRoot();
  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(exprNode.t));
  auto varDeclNode = std::get<Fanta::AST::VariableDecl>(exprNode.t);
  auto rhsVal = std::get<Fanta::AST::BinaryOperator>(
      pExpr.getNodeAtIndex(varDeclNode.defineNode).t);

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

TEST_CASE("Basic Function Expression With Return") {
  Parser pExpr{"fn hello(a : int, b : int) -> int { \
    let d : int = 10; \
    d = d + 1;\
    return d; \
  } "};
  pExpr.walk();
  auto exprNode = pExpr.getCurrentRoot();
  auto func = std::get<Fanta::AST::FunctionDef>(exprNode.t);
  auto body =
      std::get<Fanta::AST::FunctionBody>(pExpr.getNodeAtIndex(func.body).t);

  bool hasRet = false;
  for (auto &b : body.expressions) {
    if (std::holds_alternative<Fanta::AST::ReturnVal>(
            pExpr.getNodeAtIndex(b).t)) {
      hasRet = true;
      auto ret = std::get<Fanta::AST::ReturnVal>(pExpr.getNodeAtIndex(b).t);
      auto val = std::get<Fanta::AST::Identifier>(
          pExpr.getNodeAtIndex(ret.returnVal).t);
      REQUIRE_SAME("d", val.name);
    }
  }
  REQUIRE_TRUE(hasRet);
}
