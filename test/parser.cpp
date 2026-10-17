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

TEST_CASE("Precedence - Mult Binds Tighter Than Plus") {
  Parser p{"x = 1 + 2 * 3;"};
  p.walk();
  auto assign = std::get<Fanta::AST::BinaryOperator>(p.getCurrentRoot().t);
  REQUIRE_SAME(Lexer::TokenType::Equal, assign.type);
  auto plus =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(assign.rhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::Plus, plus.type);
  auto mult =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(plus.rhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::Mult, mult.type);
}

TEST_CASE("Precedence - Minus Is Left Associative") {
  Parser p{"let r : int = a - b - c;"};
  p.walk();
  auto decl = std::get<Fanta::AST::VariableDecl>(p.getCurrentRoot().t);
  auto outer =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(decl.defineNode).t);
  REQUIRE_SAME(Lexer::TokenType::Minus, outer.type);
  auto inner =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(outer.lhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::Minus, inner.type);
  REQUIRE_SAME("c",
               std::get<Fanta::AST::Identifier>(p.getNodeAtIndex(outer.rhsOp).t)
                   .name);
}

TEST_CASE("Precedence - Comparison Binds Tighter Than && / ||") {
  Parser p{"x = a > b && c < d || e == f;"};
  p.walk();
  auto assign = std::get<Fanta::AST::BinaryOperator>(p.getCurrentRoot().t);
  auto orOp =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(assign.rhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::Or, orOp.type);
  auto andOp =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(orOp.lhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::And, andOp.type);
  auto eq =
      std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(orOp.rhsOp).t);
  REQUIRE_SAME(Lexer::TokenType::EqualComp, eq.type);
}

TEST_CASE("For Loop AST Shape") {
  Parser p{"fn main() -> int { for (let i: int = 0; i < 5; i = i + 1) { x = 1; "
           "} }"};
  p.fullWalk();
  auto fn = std::get<Fanta::AST::FunctionDef>(p.getCurrentRoot().t);
  auto body = std::get<Fanta::AST::FunctionBody>(p.getNodeAtIndex(fn.body).t);
  REQUIRE_SAME(1, body.expressions.size());
  auto fr = std::get<Fanta::AST::For>(p.getNodeAtIndex(body.expressions[0]).t);
  REQUIRE_TRUE(std::holds_alternative<Fanta::AST::VariableDecl>(
      p.getNodeAtIndex(fr.init).t));
  REQUIRE_SAME(Lexer::TokenType::Lesser,
               std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(fr.comp).t)
                   .type);
  REQUIRE_SAME(Lexer::TokenType::Equal,
               std::get<Fanta::AST::BinaryOperator>(p.getNodeAtIndex(fr.incr).t)
                   .type);
}
