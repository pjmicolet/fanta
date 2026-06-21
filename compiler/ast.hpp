#pragma once
#include <cstddef>
#include <variant>
#include <vector>

#include "lexer.hpp"

namespace Fanta {
namespace AST {
using NodeIndex = std::size_t;

struct BinaryOperator {
  NodeIndex lhsOp;
  NodeIndex rhsOp;
  Lexer::TokenType type;
};

struct IntLiteral {
  int literal;
};

struct Identifier {
  std::string_view name;
};

struct VariableDecl {
  std::string_view name;
  std::string_view type;
  NodeIndex defineNode;
};

struct FunctionCall {
  NodeIndex funcNameIdx;
  std::vector<NodeIndex> params;
};

struct FunctionDef {
  std::string_view name;
  std::vector<NodeIndex> params;
  NodeIndex body;
  std::string_view retType;
};

struct FunctionParamDef {
  std::string_view name;
  std::string_view type;
};

struct FunctionBody {
  std::vector<NodeIndex> expressions;
};

using AstNodeType =
    std::variant<BinaryOperator, IntLiteral, Identifier, VariableDecl,
                 FunctionCall, FunctionDef, FunctionParamDef, FunctionBody>;

struct AstNode {
  AstNodeType t;
  int line = 0;
};
}; // namespace AST
}; // namespace Fanta
