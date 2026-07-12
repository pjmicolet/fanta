#include "parser.hpp"
#include <charconv>
#include <fanta_utils.hpp>
#include <print>

Parser::Parser(std::string program) : program_(program), lexer(program_) {
  currentToken = *lexer.getToken();
  peekToken = *lexer.getToken(); // TODO errors
}

auto Parser::walk() -> void {
  if (currentToken.t == Lexer::TokenType::Eof)
    return;
  if (accept(Lexer::TokenType::KeywordLet)) {
    auto idx = walkLet();
    expect(Lexer::TokenType::SemiColon);
    roots.push_back(idx);
  } else if (accept(Lexer::TokenType::KeywordFn)) {
    walkFn();
  } else {
    auto expr = walkExpression(Precedence::LOWEST);
    auto good = expect(Lexer::TokenType::SemiColon);
    roots.push_back(expr);
  }
}

auto Parser::fullWalk() -> void {
  while (currentToken.t != Lexer::TokenType::Eof) {
    walk();
  }
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
  std::string pad(indent * 2, ' ');

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
                          for (auto &c : fc.params) {
                            printAST(c, indent + 1);
                          }
                        },
                        [&](const Fanta::AST::FunctionDef &fc) {
                          std::println("{}FunctionDef({} -> {})", pad,
                                       fc.name, fc.retType);
                          for (auto &p : fc.params) {
                            printAST(p, indent + 1);
                          }
                          printAST(fc.body);
                        },
                        [&](const Fanta::AST::FunctionParamDef &p) {
                          std::println("{}FunctionParamDef({}:{})", pad,
                                       p.name, p.type);
                        },
                        [&](const Fanta::AST::FunctionBody &p) {
                          std::println("{}FunctionBody", pad);
                          for (auto &b : p.expressions) {
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
                        [](const auto &id) {}},
             node.t);
}

auto Parser::nextToken() -> void {
  currentToken = peekToken;
  peekToken = *lexer.getToken();
}

#define EXTRACT_TOKEN_TO_VAR(name, type)                                     \
  auto name = expect(type);                                                  \
  if (!name.has_value())                                                     \
    return;

#define EXTRACT_TOKEN_TO_VAR_WRET(name, type, ret_val)                       \
  auto name = expect(type);                                                  \
  if (!name.has_value())                                                     \
    return ret_val;

#define EXTRACT_TOKEN(type)                                                  \
  if (!expect(type).has_value())                                            \
    return -1;

auto Parser::walkLet() -> Fanta::AST::NodeIndex {
  EXTRACT_TOKEN_TO_VAR_WRET(identifier, Lexer::TokenType::Identifier, -1);
  EXTRACT_TOKEN(Lexer::TokenType::Colon);
  EXTRACT_TOKEN_TO_VAR_WRET(type, Lexer::TokenType::Type, -1);
  EXTRACT_TOKEN(Lexer::TokenType::Equal);
  auto index = walkExpression(Precedence::ASSIGN_OR_RET);
  Fanta::AST::VariableDecl var{identifier->lexeme, type->lexeme, index};
  asts.push_back({var, 0});
  return asts.size() - 1;
}

auto Parser::walkFn() -> void {
  EXTRACT_TOKEN_TO_VAR(identifier, Lexer::TokenType::Identifier);

  auto paramStart =
      expect(Lexer::TokenType::OpenParam); // TODO break if not there;

  Fanta::AST::FunctionDef func{};
  func.name = identifier->lexeme;
  while (currentToken.t != Lexer::TokenType::CloseParam) {
    auto paramName = expect(Lexer::TokenType::Identifier);
    if (paramName) {
      auto colon = expect(Lexer::TokenType::Colon);
      EXTRACT_TOKEN_TO_VAR(type, Lexer::TokenType::Type);
      Fanta::AST::FunctionParamDef def{paramName->lexeme, type->lexeme};
      asts.push_back({def, 0});
      func.params.push_back(asts.size() - 1);
      auto c = expect(Lexer::TokenType::Comma);
    }
  }
  auto close = expect(Lexer::TokenType::CloseParam);

  auto arrowType = expect(Lexer::TokenType::Arrow);

  EXTRACT_TOKEN_TO_VAR(funcRet, Lexer::TokenType::Type);

  func.retType = funcRet->lexeme;
  func.body = walkBody();
  asts.push_back({func, 0});
  roots.push_back(asts.size() - 1);
}

auto Parser::walkReturn() -> Fanta::AST::NodeIndex {
  auto index = walkExpression(Precedence::ASSIGN_OR_RET);
  Fanta::AST::ReturnVal var{"unknown",
                            index}; // we don't know the return type yet
  asts.push_back({var, 0});
  return asts.size() - 1;
}

auto Parser::walkFor() -> Fanta::AST::NodeIndex {
  Fanta::AST::For fr{};

  auto start = expect(Lexer::TokenType::OpenParam);
  fr.init = walkExpression(Precedence::LOWEST);
  fr.comp = walkExpression(Precedence::LOWEST);
  fr.incr = walkExpression(Precedence::LOWEST);

  auto endOfCondition = expect(Lexer::TokenType::CloseParam);

  fr.body = walkBody();

  asts.push_back({fr, 0});
  return asts.size() - 1;
}

auto Parser::walkBody() -> Fanta::AST::NodeIndex {
  auto start = expect(Lexer::TokenType::OpenBrace);
  Fanta::AST::FunctionBody body;
  bool hasReturn = false;
  while (currentToken.t != Lexer::TokenType::CloseBrace) {
    if (accept(Lexer::TokenType::KeywordLet)) {
      body.expressions.push_back(walkLet());
    } else if (accept(Lexer::TokenType::Return)) {
      hasReturn = true;
      auto idx = walkReturn();
      body.expressions.push_back(idx);
    } else if (accept(Lexer::TokenType::If)) {
      auto idx = walkIf();
      body.expressions.push_back(idx);
    } else if (accept(Lexer::TokenType::For)) {
      auto idx = walkFor();
      body.expressions.push_back(idx);
    } else {
      auto idx = walkExpression(Precedence::LOWEST);
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
  auto end = expect(Lexer::TokenType::CloseBrace);
  asts.push_back({body, 0});
  return asts.size() - 1;
}

auto Parser::walkIf() -> Fanta::AST::NodeIndex {
  Fanta::AST::IfStm ifStm{};
  auto openParam = expect(Lexer::TokenType::OpenParam);
  if (!openParam.has_value()) {
    return -1;
  }
  auto condStatement = walkExpression(Precedence::LOWEST);
  auto closeParam = expect(Lexer::TokenType::CloseParam);
  std::println("OK");
  auto body = walkBody();
  if (accept(Lexer::TokenType::Else)) {
    auto elseBody = walkBody();
    ifStm.elbody = elseBody;
  }
  ifStm.cond = condStatement;
  ifStm.body = body;
  asts.push_back({ifStm, 0});
  return asts.size() - 1;
}

auto Parser::walkExpression(Precedence p) -> Fanta::AST::NodeIndex {
  auto prefix = walkPrefix();
  while (p < getPrecedence(currentToken.t)) {
    auto current = currentToken;
    nextToken();
    prefix = walkInfix(prefix, current);
  }
  return prefix;
}

auto Parser::walkInfix(Fanta::AST::NodeIndex idx, Lexer::Token t)
    -> Fanta::AST::NodeIndex {
  if (t.t == Lexer::TokenType::OpenParam) {
    std::vector<Fanta::AST::NodeIndex> args;
    while (currentToken.t != Lexer::TokenType::CloseParam) {
      auto expr = walkExpression(Precedence::LOWEST);
      args.push_back(expr);
      auto c = expect(Lexer::TokenType::Comma);
    }
    auto b = expect(Lexer::TokenType::CloseParam);
    Fanta::AST::FunctionCall fc{idx, args};
    asts.push_back({fc, 0});
    return asts.size() - 1;
  } else {
    auto rhsExpr = walkExpression(getPrecedence(t.t));
    Fanta::AST::BinaryOperator op{idx, rhsExpr, t.t};
    asts.push_back({op, 0});
    return asts.size() - 1;
  }
}

auto Parser::walkPrefix() -> Fanta::AST::NodeIndex {
  switch (currentToken.t) {
  case Lexer::TokenType::Identifier: {
    EXTRACT_TOKEN_TO_VAR_WRET(rhsIdentifier, Lexer::TokenType::Identifier,
                              -1);
    Fanta::AST::Identifier id{rhsIdentifier->lexeme};
    asts.push_back({id, 0});
    return asts.size() - 1;
  }
  case Lexer::TokenType::IntLiteral: {
    EXTRACT_TOKEN_TO_VAR_WRET(rhsLiteral, Lexer::TokenType::IntLiteral, -1);
    int val = 0;
    std::from_chars(rhsLiteral->lexeme.data(),
                    rhsLiteral->lexeme.data() + rhsLiteral->lexeme.size(),
                    val);
    Fanta::AST::IntLiteral id{val};
    asts.push_back({id, 0});
    return asts.size() - 1;
  }
  case Lexer::TokenType::OpenParam: {
    nextToken();
    auto expr = walkExpression(Precedence::LOWEST);
    auto close = expect(Lexer::TokenType::CloseParam);
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
    return Precedence::ASSIGN_OR_RET;
  case Lexer::TokenType::Equal:
    return Precedence::ASSIGN_OR_RET;
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
  return std::unexpected(ParserError{});
}
