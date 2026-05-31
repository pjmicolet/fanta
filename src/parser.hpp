#pragma once
#include <string>
#include "lexer.hpp"
#include "ast.hpp"
#include <vector>
#include <charconv>

struct ParserError {
};

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

private:
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
    auto index = walkLetRhs();
    Fanta::AST::VariableDecl var{identifier->lexeme, type->lexeme, index};
    asts.push_back({var,0});
    roots.push_back(asts.size()-1);
  }

  auto walkLetRhs() -> Fanta::AST::NodeIndex {
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
      default:
        return -1;
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
