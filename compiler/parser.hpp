#pragma once
#include "ast.hpp"
#include "lexer.hpp"
#include <expected>
#include <string>
#include <vector>

struct ParserError {};

struct Parser {
  Parser(std::string program);

  auto walk() -> void;
  auto fullWalk() -> void;
  auto getCurrentRoot() const -> Fanta::AST::AstNode;
  auto getRootIndices() const -> const std::vector<Fanta::AST::NodeIndex> &;
  auto getNodeAtIndex(Fanta::AST::NodeIndex idx) const -> Fanta::AST::AstNode;
  auto tokenTypeToString(Lexer::TokenType t) -> std::string_view;
  auto dumpNodes() -> void;
  auto printAST(Fanta::AST::NodeIndex idx, int indent = 0) -> void;

private:
  enum class Precedence : uint8_t {
    LOWEST = 0,
    IFELSE = 1,
    ASSIGN_OR_RET = 2,
    LOGIC = 3,
    COMP = 4,
    SUM = 5,
    MINUS = 5, // Same precedence as SUM (left-associative +/-)
    MULT = 6,  // Should collapse mult-divide to one
    DIVIDE = 6,
    CALL = 8,
  };

  std::string program_;
  Lexer lexer;
  Lexer::Token currentToken;
  Lexer::Token peekToken;
  std::vector<Fanta::AST::AstNode> asts;
  std::vector<Fanta::AST::NodeIndex> roots;

  size_t index = 0;

  auto nextToken() -> void;

  auto walkLet() -> Fanta::AST::NodeIndex;
  auto walkFn() -> void;
  auto walkReturn() -> Fanta::AST::NodeIndex;
  auto walkFor() -> Fanta::AST::NodeIndex;
  auto walkBody() -> Fanta::AST::NodeIndex;
  auto walkIf() -> Fanta::AST::NodeIndex;

  /**
   * @brief Starting point of pratt parser to deal with operand precedence.
   *
   * @details Expressions start with lowest precedence, using walkInfix we
   * recursively descend through the expression
   *
   * To give an example of how it works take 3 * 2 + 1, we'll have:
   *  walkExpression(p=LOWEST)
   *   prefix = 3
   *   p < getPrecedence(*)
   *   walkInfix(3, *)
   *    walkExpression(p=precedence(*))
   *     prefix = 2
   *     p < getPrecedence(+) // BREAKS LOOP
   *  p < getPrecedence(+)
   *    walkInfix(*, +)
   *  DONE
   *
   * @param p: precedence of incoming token
   */
  auto walkExpression(Precedence p) -> Fanta::AST::NodeIndex;
  auto walkInfix(Fanta::AST::NodeIndex idx, Lexer::Token t)
      -> Fanta::AST::NodeIndex;
  auto walkPrefix() -> Fanta::AST::NodeIndex;
  auto getPrecedence(Lexer::TokenType t) -> Precedence;

  auto accept(Lexer::TokenType t) -> bool;
  auto expect(Lexer::TokenType t) -> std::expected<Lexer::Token, ParserError>;
};
