#pragma once
#include <cstddef>
#include "lexer.hpp"

namespace Fanta {
namespace AST {
  using NodeIndex = std::size_t;

  struct BinaryOperator {
    NodeIndex lhsOp;
    NodeIndex rhsOp;
    Lexer::TokenType type;
  };
};
};
