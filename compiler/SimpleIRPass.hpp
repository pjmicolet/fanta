#pragma once

#include "PassDefinition.hpp"
#include "ast.hpp"
#include "ir.hpp"

namespace Fanta {

struct SimpleIRPass {
  auto outputIR(Parser &p, GlobalTable &gt) -> IR;

private:
  auto emitGlobalVariableIR(const Parser &p, const AST::VariableDecl &decl,
                            FunctionIR &ir, GlobalTable &gt) -> void;
  auto emitVariableIR(const Parser &p, const AST::VariableDecl &decl,
                      IRListing &ir, const GlobalTable &gt, LocalTable &lt)
      -> void;
  auto emitReturnIR(const Parser &p, const AST::ReturnVal &decl, IRListing &ir,
                    const GlobalTable &gt, LocalTable &lt) -> void;
  auto emitFunctionDef(const Parser &p, const AST::FunctionDef &decl, IR &ir,
                       const GlobalTable &gt) -> void;
  auto emitStatement(const Parser &p, const AST::AstNode &node, IRListing &ir,
                     const GlobalTable &gt, LocalTable &lt) -> void;
  auto emitBinaryOpIR(const AST::BinaryOperator &bOp, IRListing &ir,
                      LocalTable &lt) -> void;
  auto emitGlobalNameBase(IRListing &ir, const GlobalTable &gt, LocalTable &lt)
      -> TempReg;
  auto emitFunctionPrelude(const Parser &p, const AST::FunctionDef &decl,
                           FunctionIR &ir, LocalTable &lt) -> void;
  auto emitExpression(const Parser &p, const AST::AstNode &node, IRListing &ir,
                      const GlobalTable &gt, LocalTable &lt, TempReg tr)
      -> void;
  auto emitCall(const Parser &p, const AST::FunctionCall &node, IRListing &ir,
                const GlobalTable &gt, LocalTable &lt) -> void;
  auto emitIf(const Parser &p, const AST::IfStm &ifStm, IRListing &ir,
              const GlobalTable &gt, LocalTable &lt) -> void;
  uint32_t globalOffsets = 0;

  auto handleLogicalCheck(const Parser &p, const AST::BinaryOperator &bop,
                          IRListing &ir, const GlobalTable &gt, LocalTable &lt,
                          Lexer::TokenType type, std::string exitLabel,
                          std::string earlyEntry) -> void;

  /**
   * Nested Logical if statements have a very specific check for and || or
   * basically when you have an and, if the lhs is false just skip checking rhs
   * since it won't matter
   *
   * For or, both sides can potentially let us jump to the happy path so check
   * both for true.
   *
   *  This handles cases such as:
   *
   *   if( (a > b && c > d) || (e > f))
   *               ^
   *               |_ finds itself on the emitCondTrue side
   *                    this means if a>b is bad, we can skip checking c > d and
   *                    just check e>f
   *                    however if a>b && c>d are good then skip e>f since we're
   *                    golden.
   */
  auto handleNestedCondTrueLogicalCheck(const Parser &p,
                                        const AST::BinaryOperator &bop,
                                        IRListing &ir, const GlobalTable &gt,
                                        LocalTable &lt, Lexer::TokenType type,
                                        std::string happyPath) -> void;

  auto emitComparison(const Parser &p, const AST::BinaryOperator &node,
                      IRListing &ir, const GlobalTable &gt, LocalTable &lt)
      -> void;
  auto emitCondTrue(const Parser &p, const AST::AstNode &node, IRListing &ir,
                    const GlobalTable &gt, LocalTable &lt,
                    std::string jumpLabel) -> void;
  auto emitCondFalse(const Parser &p, const AST::AstNode &node, IRListing &ir,
                     const GlobalTable &gt, LocalTable &lt,
                     std::string jumpLabel) -> void;
  auto emitConditionalBranch(const Parser &p, const AST::BinaryOperator &node,
                             IRListing &ir, const GlobalTable &gt,
                             LocalTable &lt, std::string label) -> void;
};

} // namespace Fanta
