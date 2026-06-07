#pragma once

#include "PassDefinition.hpp"
#include "ast.hpp"
#include "ir.hpp"

namespace Fanta {

struct SimpleIRPass {
  auto outputIR(Parser &p, GlobalTable &gt) -> IR;

private:
  auto emitGlobalVariableIR(const Parser &p, const AST::VariableDecl &decl,
                            IR &ir) -> void;
  auto emitVariableIR(const Parser &p, const AST::VariableDecl &decl, IR &ir,
                      const GlobalTable &gt, LocalTable &lt) -> void;
  auto emitFunctionDef(const Parser &p, const AST::FunctionDef &decl, IR &ir,
                       const GlobalTable &gt) -> void;
  auto emitBinaryOpIR(const AST::BinaryOperator &bOp, IR &ir, LocalTable &lt)
      -> void;
  auto emitGlobalNameBase(IR &ir, const GlobalTable &gt, LocalTable &lt)
      -> TempReg;
};

} // namespace Fanta
