#pragma once
#include <string>
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <charconv>
#include <print>

struct ParserError {
};

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

struct Parser {
  Parser(std::string program) : program_(program), lexer(program_) {
    currentToken = *lexer.getToken();
    peekToken = *lexer.getToken(); // TODO errors
  }

  auto walk() -> void {
    if(accept(Lexer::TokenType::KeywordLet)) {
      walkLet();
    }
  }

  auto getCurrentRoot() const -> Fanta::AST::AstNode {
    return asts[roots.back()];
  }

  auto getNodeAtIndex(Fanta::AST::NodeIndex idx) -> Fanta::AST::AstNode {
    return asts[idx];
  }

  constexpr auto tokenTypeToString(Lexer::TokenType t) -> std::string_view {
      switch (t) {
          case Lexer::TokenType::Plus:      return "+";
          case Lexer::TokenType::Minus:     return "-";
          case Lexer::TokenType::Mult:      return "*";
          case Lexer::TokenType::Slash:     return "/";
          case Lexer::TokenType::KeywordLet: return "let";
          case Lexer::TokenType::KeywordFn:  return "fn";
          case Lexer::TokenType::Identifier: return "Identifier";
          case Lexer::TokenType::IntLiteral: return "IntLiteral";
          default:                           return "Unknown";
      }
  }

  auto dumpNodes() -> void {
    for(auto& rootId : roots) {
      printAST(rootId);
    }
  }

  auto printAST(Fanta::AST::NodeIndex idx, int indent = 0) -> void {
    std::string pad(indent*2, ' ');

    const auto& node = asts[idx];
    std::visit(overloaded{
      [&](const Fanta::AST::IntLiteral& lit) { std::println("{}Literal({})",pad,lit.literal); },
      [&](const Fanta::AST::Identifier& lit) { std::println("{}Identifier({})",pad,lit.name); },
      [&](const Fanta::AST::VariableDecl& decl) { std::println("{}Declare({})",pad,decl.name); 
        printAST(decl.defineNode, indent+1);
      },
      [&](const Fanta::AST::BinaryOperator& binaryOp) { 
        std::println("{}BinaryOperator({})",pad,tokenTypeToString(binaryOp.type));
        printAST(binaryOp.lhsOp, indent+1);
        printAST(binaryOp.rhsOp,indent+1);
      }
    }, node.t);
  }

private:

  enum class Precedence : uint8_t { 
    LOWEST = 0,
    ASSIGN = 1,
    SUM = 2,
    MINUS = 2,
    MULT = 3,
    DIVIDE = 3,
    CALL = 4
  };

  std::string program_;
  Lexer lexer;
  Lexer::Token currentToken;
  Lexer::Token peekToken;
  std::vector<Fanta::AST::AstNode> asts;
  std::vector<Fanta::AST::NodeIndex> roots;

  size_t index = 0;

  auto nextToken() -> void {
    currentToken = peekToken;
    peekToken = *lexer.getToken();
  }

  #define EXTRACT_TOKEN_TO_VAR(name, type)\
       auto name = expect(type);\
       if(!name.has_value()) return;\

  #define EXTRACT_TOKEN_TO_VAR_WRET(name, type, ret_val)\
       auto name = expect(type);\
       if(!name.has_value()) return ret_val;\

  #define EXTRACT_TOKEN(type)\
       if(!expect(type).has_value()) return;\

  auto walkLet() -> void {
    EXTRACT_TOKEN_TO_VAR(identifier, Lexer::TokenType::Identifier);
    EXTRACT_TOKEN(Lexer::TokenType::Colon);
    EXTRACT_TOKEN_TO_VAR(type, Lexer::TokenType::Type);
    EXTRACT_TOKEN(Lexer::TokenType::Equal);
    auto index = walkExpression(Precedence::ASSIGN);
    Fanta::AST::VariableDecl var{identifier->lexeme, type->lexeme, index};
    asts.push_back({var,0});
    roots.push_back(asts.size()-1);
  }

  auto walkExpression(Precedence p) -> Fanta::AST::NodeIndex {
    auto prefix = walkPrefix();

    while(p < getPrecedence(currentToken.t)) {
      auto current = currentToken;
      nextToken();
      prefix = walkInfix(prefix, current);
    }
    return prefix;
  }

  auto walkInfix(Fanta::AST::NodeIndex idx, Lexer::Token t) -> Fanta::AST::NodeIndex {
    auto rhsExpr = walkExpression(getPrecedence(t.t));
    Fanta::AST::BinaryOperator op{idx,rhsExpr,t.t};
    asts.push_back({op,0});
    return asts.size()-1;
  }
    
  auto walkPrefix() -> Fanta::AST::NodeIndex {
    switch(currentToken.t) {
      case Lexer::TokenType::Identifier: {
        EXTRACT_TOKEN_TO_VAR_WRET(rhsIdentifier, Lexer::TokenType::Identifier, -1);
        Fanta::AST::Identifier id{rhsIdentifier->lexeme};
        asts.push_back({id,0});
        return asts.size()-1;
      }
      case Lexer::TokenType::IntLiteral: {
        EXTRACT_TOKEN_TO_VAR_WRET(rhsLiteral, Lexer::TokenType::IntLiteral, -1);
        int val = 0;
        std::from_chars(rhsLiteral->lexeme.data(), rhsLiteral->lexeme.data()+rhsLiteral->lexeme.size(), val);
        Fanta::AST::IntLiteral id{val};
        asts.push_back({id,0});
        return asts.size()-1;
      }
      case Lexer::TokenType::SemiColon: {
        return -1;
      }
      default:
        return -1;
    }
  }

  auto getPrecedence(Lexer::TokenType t) -> Precedence {
    switch(t) {
      case Lexer::TokenType::KeywordLet: return Precedence::ASSIGN;
      case Lexer::TokenType::Equal: return Precedence::ASSIGN;
      case Lexer::TokenType::Mult: return Precedence::MULT;
      case Lexer::TokenType::Minus: return Precedence::MINUS;
      case Lexer::TokenType::Plus: return Precedence::SUM;
      case Lexer::TokenType::SemiColon: return Precedence::LOWEST;
      case Lexer::TokenType::Slash: return Precedence::DIVIDE;
      default:
        return Precedence::LOWEST;
    }
  }

  auto accept(Lexer::TokenType t) -> bool {
    if(currentToken.t == t) {
      nextToken();
      return true;
    }
    return false;
  }

  auto expect(Lexer::TokenType t) -> std::expected<Lexer::Token, ParserError> {
    if(currentToken.t == t) {
      auto token = currentToken;
      nextToken();
      return token;
    }
    return std::unexpected(ParserError{});
  }
};
