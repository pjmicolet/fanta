#pragma once
#include <cstddef>
#include <variant>

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

  using AstNodeType = std::variant<BinaryOperator, IntLiteral, Identifier, VariableDecl>;
  
  struct AstNode {
    AstNodeType t;
    int line = 0;
  };
};
};
