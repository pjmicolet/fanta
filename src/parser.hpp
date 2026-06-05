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
    if(currentToken.t == Lexer::TokenType::Eof) return;
    if(accept(Lexer::TokenType::KeywordLet)) {
      roots.push_back(walkLet());
    }
    else if(accept(Lexer::TokenType::KeywordFn)) {
      walkFn();
    } else {
      auto expr = walkExpression(Precedence::LOWEST);
      auto good = expect(Lexer::TokenType::SemiColon);
      roots.push_back(expr);
    }
  }

  auto fullWalk() -> void {
    while(currentToken.t != Lexer::TokenType::Eof) {
      walk();
    }
  }

  auto getCurrentRoot() const -> Fanta::AST::AstNode {
    return asts[roots.back()];
  }

  auto getRootIndices() const -> const std::vector<Fanta::AST::NodeIndex>& {
    return roots;
  }

  auto getNodeAtIndex(Fanta::AST::NodeIndex idx) const -> Fanta::AST::AstNode {
    return asts[idx];
  }

  constexpr auto tokenTypeToString(Lexer::TokenType t) -> std::string_view {
      switch (t) {
          case Lexer::TokenType::Plus:      return "+";
          case Lexer::TokenType::Equal:      return "=";
          case Lexer::TokenType::Minus:     return "-";
          case Lexer::TokenType::Mult:      return "*";
          case Lexer::TokenType::Slash:     return "/";
          case Lexer::TokenType::KeywordLet: return "let";
          case Lexer::TokenType::KeywordFn:  return "fn";
          case Lexer::TokenType::Identifier: return "Identifier";
          case Lexer::TokenType::IntLiteral: return "IntLiteral";
          case Lexer::TokenType::Colon: return ":";
          case Lexer::TokenType::OpenBrace: return "{";
          case Lexer::TokenType::CloseBrace: return "}";
          case Lexer::TokenType::SemiColon: return ";";
          case Lexer::TokenType::Arrow: return "->";
          case Lexer::TokenType::Comma: return ",";
          case Lexer::TokenType::OpenParam: return "(";
          case Lexer::TokenType::CloseParam: return ")";
          case Lexer::TokenType::Type: return "Type";
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
      },
      [&](const Fanta::AST::FunctionCall& fc) { 
        std::println("{}FunctionCall()",pad);
        printAST(fc.funcNameIdx, indent+1);
        for(auto& c : fc.params) {
          printAST(c, indent+1);
        }
      },
      [&](const Fanta::AST::FunctionDef& fc) { 
        std::println("{}FunctionDef({} -> {})",pad, fc.name, fc.retType);
        for(auto& p : fc.params) {
          printAST(p, indent+1);
        }
        printAST(fc.body);
      },
      [&](const Fanta::AST::FunctionParamDef& p) {
        std::println("{}FunctionParamDef({}:{})",pad, p.name, p.type);
      },
      [&](const Fanta::AST::FunctionBody& p) {
        std::println("{}FunctionBody",pad);
        for(auto& b : p.expressions) {
          printAST(b, indent+1);
        }
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
       if(!expect(type).has_value()) return -1;\

  auto walkLet() -> Fanta::AST::NodeIndex {
    EXTRACT_TOKEN_TO_VAR_WRET(identifier, Lexer::TokenType::Identifier, -1);
    EXTRACT_TOKEN(Lexer::TokenType::Colon);
    EXTRACT_TOKEN_TO_VAR_WRET(type, Lexer::TokenType::Type, -1);
    EXTRACT_TOKEN(Lexer::TokenType::Equal);
    auto index = walkExpression(Precedence::ASSIGN);
    Fanta::AST::VariableDecl var{identifier->lexeme, type->lexeme, index};
    asts.push_back({var,0});
    return asts.size() -1;
  }

  auto walkFn() -> void {
    EXTRACT_TOKEN_TO_VAR(identifier, Lexer::TokenType::Identifier);

    auto paramStart = expect(Lexer::TokenType::OpenParam); // TODO break if not there;

    Fanta::AST::FunctionDef func{};
    func.name = identifier->lexeme;
    while(currentToken.t != Lexer::TokenType::CloseParam) {
      auto paramName = expect(Lexer::TokenType::Identifier);
      if(paramName) {
        auto colon = expect(Lexer::TokenType::Colon);
        EXTRACT_TOKEN_TO_VAR(type, Lexer::TokenType::Type);
        Fanta::AST::FunctionParamDef def{paramName->lexeme, type->lexeme};
        asts.push_back({def, 0});
        func.params.push_back(asts.size()-1);
        auto c = expect(Lexer::TokenType::Comma);
      }
    }
    auto close = expect(Lexer::TokenType::CloseParam);

    auto arrowType = expect(Lexer::TokenType::Arrow);

    EXTRACT_TOKEN_TO_VAR(funcRet, Lexer::TokenType::Type);

    func.retType = funcRet->lexeme;
    func.body = walkBody();
    asts.push_back({func,0});
    roots.push_back(asts.size()-1);
  }

  auto walkBody() -> Fanta::AST::NodeIndex {
    auto start = expect(Lexer::TokenType::OpenBrace);
    Fanta::AST::FunctionBody body;
    while(currentToken.t != Lexer::TokenType::CloseBrace){
      if(accept(Lexer::TokenType::KeywordLet)) {
        body.expressions.push_back(walkLet());
      } else {
        auto idx = walkExpression(Precedence::LOWEST);
        body.expressions.push_back(idx);
      }
      auto sc = expect(Lexer::TokenType::SemiColon);
    }
    auto end = expect(Lexer::TokenType::CloseBrace);
    asts.push_back({body, 0});
    return asts.size() - 1;
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
    if(t.t == Lexer::TokenType::OpenParam) {
      std::vector<Fanta::AST::NodeIndex> args;
      while(currentToken.t != Lexer::TokenType::CloseParam) {
        auto expr = walkExpression(Precedence::LOWEST);
        args.push_back(expr);
        auto c = expect(Lexer::TokenType::Comma);
      }
      auto b = expect(Lexer::TokenType::CloseParam);
      Fanta::AST::FunctionCall fc{idx, args};
      asts.push_back({fc,0});
      return asts.size()-1;
    } else {
      auto rhsExpr = walkExpression(getPrecedence(t.t));
      Fanta::AST::BinaryOperator op{idx,rhsExpr,t.t};
      asts.push_back({op,0});
      return asts.size()-1;
    }
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
      case Lexer::TokenType::OpenParam: return Precedence::CALL;
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
