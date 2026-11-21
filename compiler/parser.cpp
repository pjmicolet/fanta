#include "parser.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include <charconv>
#include <expected>
#include <fanta_utils.hpp>
#include <format>
#include <print>
#include <utility>

#define TRY(var, expr)                                                         \
  auto var##_r = (expr);                                                       \
  if (!var##_r)                                                                \
    return std::unexpected(var##_r.error());                                   \
  auto var = *var##_r;

Parser::Parser(std::string program)
    : program_(std::move(program)), lexer(program_) {
  currentToken = *lexer.getToken();
  peekToken = *lexer.getToken(); // TODO errors
}

auto Parser::walk() -> std::expected<void, ParserError> {
  if (currentToken.t == Lexer::TokenType::Eof)
    return {};
  if (accept(Lexer::TokenType::KeywordLet)) {
    TRY(idx, walkLet());
    auto res = expect(Lexer::TokenType::SemiColon);
    if (!res) {
      return std::unexpected(res.error());
    }
    roots.push_back(idx);
  } else if (accept(Lexer::TokenType::KeywordFn)) {
    if (auto fn = walkFn(); !fn)
      return std::unexpected(fn.error());
  } else {
    TRY(expr, walkExpression(Precedence::LOWEST));
    auto good = expect(Lexer::TokenType::SemiColon);
    if (!good) {
      return std::unexpected(good.error());
    }
    roots.push_back(expr);
  }
  return {};
}

auto Parser::fullWalk() -> std::expected<void, ParserError> {
  while (currentToken.t != Lexer::TokenType::Eof) {
    auto res = walk();
    if (!res)
      return std::unexpected(res.error());
  }
  return {};
}

auto Parser::getCurrentRoot() const -> Fanta::AST::AstNode {
  return asts[roots.back()];
}

auto Parser::getRootIndices() const
    -> const std::vector<Fanta::AST::NodeIndex> & {
  return roots;
}

auto Parser::getNodeAtIndex(Fanta::AST::NodeIndex idx) const
    -> Fanta::AST::AstNode {
  return asts[idx];
}

auto Parser::tokenTypeToString(Lexer::TokenType t) -> std::string_view {
  switch (t) {
  case Lexer::TokenType::Plus:
    return "+";
  case Lexer::TokenType::Equal:
    return "=";
  case Lexer::TokenType::Minus:
    return "-";
  case Lexer::TokenType::Mult:
    return "*";
  case Lexer::TokenType::Slash:
    return "/";
  case Lexer::TokenType::KeywordLet:
    return "let";
  case Lexer::TokenType::KeywordFn:
    return "fn";
  case Lexer::TokenType::Identifier:
    return "Identifier";
  case Lexer::TokenType::IntLiteral:
    return "IntLiteral";
  case Lexer::TokenType::Colon:
    return ":";
  case Lexer::TokenType::OpenBrace:
    return "{";
  case Lexer::TokenType::CloseBrace:
    return "}";
  case Lexer::TokenType::SemiColon:
    return ";";
  case Lexer::TokenType::Arrow:
    return "->";
  case Lexer::TokenType::Comma:
    return ",";
  case Lexer::TokenType::OpenParam:
    return "(";
  case Lexer::TokenType::CloseParam:
    return ")";
  case Lexer::TokenType::Type:
    return "Type";
  case Lexer::TokenType::EqualComp:
    return "==";
  case Lexer::TokenType::GreaterEq:
    return ">=";
  case Lexer::TokenType::Greater:
    return ">";
  case Lexer::TokenType::LesserEq:
    return "<=";
  case Lexer::TokenType::Lesser:
    return "<";
  case Lexer::TokenType::NotEq:
    return "!=";
  case Lexer::TokenType::And:
    return "&&";
  case Lexer::TokenType::Or:
    return "||";
  case Lexer::TokenType::BitAnd:
    return "&";
  case Lexer::TokenType::BitOr:
    return "|";
  default:
    return "Unknown";
  }
}

auto Parser::dumpNodes() -> void {
  for (auto &rootId : roots) {
    printAST(rootId);
  }
}

auto Parser::printAST(Fanta::AST::NodeIndex idx, int indent) -> void {
  std::string pad(static_cast<std::size_t>(indent) * 2, ' ');

  const auto &node = asts[idx];
  std::visit(overloaded{[&](const Fanta::AST::IntLiteral &lit) {
                          std::println("{}Literal({})", pad, lit.literal);
                        },
                        [&](const Fanta::AST::Identifier &lit) {
                          std::println("{}Identifier({})", pad, lit.name);
                        },
                        [&](const Fanta::AST::VariableDecl &decl) {
                          std::println("{}Declare({})", pad, decl.name);
                          printAST(decl.defineNode, indent + 1);
                        },
                        [&](const Fanta::AST::BinaryOperator &binaryOp) {
                          std::println("{}BinaryOperator({})", pad,
                                       tokenTypeToString(binaryOp.type));
                          printAST(binaryOp.lhsOp, indent + 1);
                          printAST(binaryOp.rhsOp, indent + 1);
                        },
                        [&](const Fanta::AST::FunctionCall &fc) {
                          std::println("{}FunctionCall()", pad);
                          printAST(fc.funcNameIdx, indent + 1);
                          for (const auto &c : fc.params) {
                            printAST(c, indent + 1);
                          }
                        },
                        [&](const Fanta::AST::FunctionDef &fc) {
                          std::println("{}FunctionDef({} -> {})", pad, fc.name,
                                       fc.retType);
                          for (const auto &p : fc.params) {
                            printAST(p, indent + 1);
                          }
                          printAST(fc.body);
                        },
                        [&](const Fanta::AST::FunctionParamDef &p) {
                          std::println("{}FunctionParamDef({}:{})", pad, p.name,
                                       p.type);
                        },
                        [&](const Fanta::AST::FunctionBody &p) {
                          std::println("{}FunctionBody", pad);
                          for (const auto &b : p.expressions) {
                            printAST(b, indent + 1);
                          }
                        },
                        [&](const Fanta::AST::ReturnVal &r) {
                          std::println("{}Return<{}>:", pad, r.type);
                          printAST(r.returnVal, indent + 1);
                        },
                        [&](const Fanta::AST::IfStm &ifStm) {
                          std::println("{}If", pad);
                          printAST(ifStm.cond, indent + 1);
                          printAST(ifStm.body, indent + 2);
                          if (ifStm.elbody) {
                            std::println("{}Else", pad);
                            printAST(*ifStm.elbody, indent + 1);
                          }
                        },
                        [&](const Fanta::AST::For &forStm) {
                          std::println("{}For", pad);
                          std::println("{}init:", pad);
                          printAST(forStm.init, indent + 1);
                          std::println("{}comp:", pad);
                          printAST(forStm.comp, indent + 1);
                          std::println("{}incr:", pad);
                          printAST(forStm.incr, indent + 1);
                          std::println("{}body:", pad);
                          printAST(forStm.body, indent + 2);
                        },
                        [](const auto &) {}},
             node.t);
}

auto Parser::nextToken() -> void {
  currentToken = peekToken;
  peekToken = *lexer.getToken();
}

#define EXTRACT_TOKEN_TO_VAR(name, type) TRY(name, expect(type))

#define EXTRACT_TOKEN_TO_VAR_WRET(name, type, ret_val) TRY(name, expect(type))

#define EXTRACT_TOKEN(type)                                                    \
  if (auto tok_r = expect(type); !tok_r)                                       \
    return std::unexpected(tok_r.error());

auto Parser::walkLet() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  EXTRACT_TOKEN_TO_VAR_WRET(identifier, Lexer::TokenType::Identifier, -1);
  EXTRACT_TOKEN(Lexer::TokenType::Colon);
  EXTRACT_TOKEN_TO_VAR_WRET(type, Lexer::TokenType::Type, -1);
  EXTRACT_TOKEN(Lexer::TokenType::Equal);
  TRY(index, walkExpression(Precedence::ASSIGN_OR_RET));
  Fanta::AST::VariableDecl var{identifier.lexeme, type.lexeme, index};
  asts.push_back({var, 0});
  return asts.size() - 1;
}

auto Parser::walkFn() -> std::expected<void, ParserError> {
  EXTRACT_TOKEN_TO_VAR(identifier, Lexer::TokenType::Identifier);

  EXTRACT_TOKEN(Lexer::TokenType::OpenParam);

  Fanta::AST::FunctionDef func{};
  func.name = identifier.lexeme;
  while (currentToken.t != Lexer::TokenType::CloseParam) {
    TRY(paramName, expect(Lexer::TokenType::Identifier));
    EXTRACT_TOKEN(Lexer::TokenType::Colon);
    EXTRACT_TOKEN_TO_VAR(type, Lexer::TokenType::Type);
    Fanta::AST::FunctionParamDef def{paramName.lexeme, type.lexeme};
    asts.push_back({def, 0});
    func.params.push_back(asts.size() - 1);
    if (currentToken.t != Lexer::TokenType::CloseParam)
      EXTRACT_TOKEN(Lexer::TokenType::Comma);
  }
  EXTRACT_TOKEN(Lexer::TokenType::CloseParam);

  EXTRACT_TOKEN(Lexer::TokenType::Arrow);

  EXTRACT_TOKEN_TO_VAR(funcRet, Lexer::TokenType::Type);

  func.retType = funcRet.lexeme;
  TRY(body, walkBody());
  func.body = body;
  asts.push_back({func, 0});
  roots.push_back(asts.size() - 1);
  return {};
}

auto Parser::walkReturn() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  TRY(index, walkExpression(Precedence::ASSIGN_OR_RET));
  Fanta::AST::ReturnVal var{
      "unknown",
      index,
  }; // we don't know the return type yet
  asts.push_back({var, 0});
  return asts.size() - 1;
}

auto Parser::walkFor() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  Fanta::AST::For fr{};

  EXTRACT_TOKEN(Lexer::TokenType::OpenParam);
  if (accept(Lexer::TokenType::KeywordLet)) {
    TRY(init, walkLet());
    fr.init = init;
  }
  EXTRACT_TOKEN(Lexer::TokenType::SemiColon);
  TRY(comp, walkExpression(Precedence::LOWEST));
  fr.comp = comp;
  EXTRACT_TOKEN(Lexer::TokenType::SemiColon);
  TRY(incr, walkExpression(Precedence::LOWEST));
  fr.incr = incr;
  accept(Lexer::TokenType::SemiColon); // Just swallow any trailing ;
  EXTRACT_TOKEN(Lexer::TokenType::CloseParam);
  TRY(body, walkBody());
  fr.body = body;

  asts.push_back({fr, 0});
  return asts.size() - 1;
}

auto Parser::walkBody() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  EXTRACT_TOKEN(Lexer::TokenType::OpenBrace);
  Fanta::AST::FunctionBody body;
  bool hasReturn = false;
  while (currentToken.t != Lexer::TokenType::CloseBrace) {
    if (accept(Lexer::TokenType::KeywordLet)) {
      TRY(idx, walkLet());
      body.expressions.push_back(idx);
    } else if (accept(Lexer::TokenType::Return)) {
      hasReturn = true;
      TRY(idx, walkReturn());
      body.expressions.push_back(idx);
    } else if (accept(Lexer::TokenType::If)) {
      TRY(idx, walkIf());
      body.expressions.push_back(idx);
    } else if (accept(Lexer::TokenType::For)) {
      TRY(idx, walkFor());
      body.expressions.push_back(idx);
    } else {
      TRY(idx, walkExpression(Precedence::LOWEST));
      if (hasReturn) {
        // We've already seen a return, anything else should be discarded.
        // That being said we should walk through the rest just so that we
        // can properly parse the function I'll write a warning here
        continue;
      }
      body.expressions.push_back(idx);
    }
    auto sc = expect(Lexer::TokenType::SemiColon);
  }
  EXTRACT_TOKEN(Lexer::TokenType::CloseBrace);
  asts.push_back({body, 0});
  return asts.size() - 1;
}

auto Parser::walkIf() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  Fanta::AST::IfStm ifStm{};
  EXTRACT_TOKEN(Lexer::TokenType::OpenParam);
  TRY(condStatement, walkExpression(Precedence::LOWEST));
  EXTRACT_TOKEN(Lexer::TokenType::CloseParam);
  TRY(body, walkBody());
  if (accept(Lexer::TokenType::Else)) {
    TRY(elseBody, walkBody());
    ifStm.elbody = elseBody;
  }
  ifStm.cond = condStatement;
  ifStm.body = body;
  asts.push_back({ifStm, 0});
  return asts.size() - 1;
}

auto Parser::walkExpression(Precedence p)
    -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  TRY(prefix, walkPrefix());
  while (p < getPrecedence(currentToken.t)) {
    auto current = currentToken;
    nextToken();
    TRY(infix, walkInfix(prefix, current));
    prefix = infix;
  }
  return prefix;
}

auto Parser::walkInfix(Fanta::AST::NodeIndex idx, Lexer::Token t)
    -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  if (t.t == Lexer::TokenType::OpenParam) {
    std::vector<Fanta::AST::NodeIndex> args;
    if (auto callArgs = walkThroughCallArgs(args); !callArgs)
      return std::unexpected(callArgs.error());
    EXTRACT_TOKEN(Lexer::TokenType::CloseParam);
    Fanta::AST::FunctionCall fc{idx, args};
    asts.push_back({fc, 0});
    return asts.size() - 1;
  } else {
    TRY(rhsExpr, walkExpression(getPrecedence(t.t)));
    Fanta::AST::BinaryOperator op{idx, rhsExpr, t.t};
    asts.push_back({op, 0});
    return asts.size() - 1;
  }
}

auto Parser::walkThroughCallArgs(std::vector<Fanta::AST::NodeIndex> &args)
    -> std::expected<void, ParserError> {
  while (currentToken.t != Lexer::TokenType::CloseParam) {
    TRY(expr, walkExpression(Precedence::LOWEST));
    // walkPrefix consumes nothing when it can't start an expression, so going
    // round again would never reach the CloseParam (or Eof).
    if (expr == static_cast<Fanta::AST::NodeIndex>(-1)) {
      return std::unexpected(
          ParserError(ParserErrorType::unexpected_char,
                      "expected an expression or ')' in call arguments",
                      currentToken.line));
    }
    args.push_back(expr);
    auto c = expect(Lexer::TokenType::Comma);
  }
  return {};
}

auto Parser::walkPrefix() -> std::expected<Fanta::AST::NodeIndex, ParserError> {
  switch (currentToken.t) {
  case Lexer::TokenType::Identifier: {
    EXTRACT_TOKEN_TO_VAR_WRET(rhsIdentifier, Lexer::TokenType::Identifier, -1);
    Fanta::AST::Identifier id{rhsIdentifier.lexeme};
    asts.push_back({id, 0});
    return asts.size() - 1;
  }
  case Lexer::TokenType::IntLiteral: {
    EXTRACT_TOKEN_TO_VAR_WRET(rhsLiteral, Lexer::TokenType::IntLiteral, -1);
    int val = 0;
    std::from_chars(rhsLiteral.lexeme.data(),
                    rhsLiteral.lexeme.data() + rhsLiteral.lexeme.size(), val);
    Fanta::AST::IntLiteral id{val};
    asts.push_back({id, 0});
    return asts.size() - 1;
  }
  case Lexer::TokenType::Intrinsic: {
    EXTRACT_TOKEN_TO_VAR_WRET(intrinsicName, Lexer::TokenType::Intrinsic, -1);
    Fanta::AST::Identifier id{intrinsicName.lexeme};
    asts.push_back({id, 0});
    auto idIndx = asts.size() - 1;
    if (!expect(Lexer::TokenType::OpenParam)) {
      return std::unexpected(ParserError(
          ParserErrorType::unexpected_char,
          "expected '(' after intrinsic " + std::string{intrinsicName.lexeme},
          currentToken.line));
    }
    std::vector<Fanta::AST::NodeIndex> args;
    if (auto callArgs = walkThroughCallArgs(args); !callArgs)
      return std::unexpected(callArgs.error());
    EXTRACT_TOKEN(Lexer::TokenType::CloseParam);
    Fanta::AST::FunctionCall fc{idIndx, args, Fanta::AST::INTRINSIC};
    asts.push_back({fc, 0});
    return asts.size() - 1;
  }
  case Lexer::TokenType::OpenParam: {
    nextToken();
    TRY(expr, walkExpression(Precedence::LOWEST));
    EXTRACT_TOKEN(Lexer::TokenType::CloseParam);
    return expr;
  }
  case Lexer::TokenType::SemiColon: {
    return -1;
  }
  default:
    return -1;
  }
}

auto Parser::getPrecedence(Lexer::TokenType t) -> Precedence {
  switch (t) {
  case Lexer::TokenType::KeywordLet:
  case Lexer::TokenType::Equal:
  case Lexer::TokenType::Return:
    return Precedence::ASSIGN_OR_RET;
  case Lexer::TokenType::OpenParam:
    return Precedence::CALL;
  case Lexer::TokenType::Mult:
    return Precedence::MULT;
  case Lexer::TokenType::Minus:
    return Precedence::MINUS;
  case Lexer::TokenType::Plus:
    return Precedence::SUM;
  case Lexer::TokenType::SemiColon:
  case Lexer::TokenType::CloseParam:
    return Precedence::LOWEST;
  case Lexer::TokenType::Slash:
    return Precedence::DIVIDE;
  case Lexer::TokenType::NotEq:
  case Lexer::TokenType::EqualComp:
  case Lexer::TokenType::Lesser:
  case Lexer::TokenType::LesserEq:
  case Lexer::TokenType::GreaterEq:
  case Lexer::TokenType::Greater:
    return Precedence::COMP;
  case Lexer::TokenType::If:
  case Lexer::TokenType::Else:
    return Precedence::IFELSE;
  case Lexer::TokenType::BitAnd:
  case Lexer::TokenType::BitOr:
    return Precedence::BITWISE;
  case Lexer::TokenType::And:
  case Lexer::TokenType::Or:
    return Precedence::LOGIC;
  default:
    return Precedence::LOWEST;
  }
}

auto Parser::accept(Lexer::TokenType t) -> bool {
  if (currentToken.t == t) {
    nextToken();
    return true;
  }
  return false;
}

auto Parser::expect(Lexer::TokenType t)
    -> std::expected<Lexer::Token, ParserError> {
  if (currentToken.t == t) {
    auto token = currentToken;
    nextToken();
    return token;
  }
  return std::unexpected(
      ParserError(ParserErrorType::unexpected_char,
                  std::format("expected '{}', got '{}'", tokenTypeToString(t),
                              currentToken.lexeme),
                  currentToken.line));
}
