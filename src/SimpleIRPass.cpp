#include "SimpleIRPass.hpp"
#include "ast.hpp"
#include "ir.hpp"
#include <variant>

namespace Fanta {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto SimpleIRPass::outputIR(Parser &p, GlobalTable &gt) -> IR {
  IR ir{};

  const auto &cr = p.getCurrentRoot();
  std::visit(overloaded{[&](const AST::VariableDecl &vdec) {
                          emitGlobalVariableIR(p, vdec, ir);
                        },
                        [&](const AST::FunctionDef &fdef) {
                          emitFunctionDef(p, fdef, ir, gt);
                        },
                        [&](const auto &other) {}},
             cr.t);
  return ir;
}

auto SimpleIRPass::emitGlobalVariableIR(const Parser &p,
                                        const AST::VariableDecl &decl, IR &ir)
    -> void {}

auto SimpleIRPass::emitFunctionDef(const Parser &p,
                                   const AST::FunctionDef &fdef, IR &irDef,
                                   const GlobalTable &gt) -> void {
  const auto bodyNode =
      std::get<AST::FunctionBody>(p.getNodeAtIndex(fdef.body).t);

  LocalTable lt{};
  FunctionIR func{};
  auto &ir = func.insts;

  func.name = fdef.name;
  emitFunctionPrelude(p, fdef, func, lt);
  for (const auto &idx : bodyNode.expressions) {
    std::visit(overloaded{[&](const AST::BinaryOperator &bOp) {
                            emitBinaryOpIR(bOp, ir, lt);
                          },
                          [&](const AST::VariableDecl &vdec) {
                            emitVariableIR(p, vdec, ir, gt, lt);
                          },
                          [&](const auto &other) {}},
               p.getNodeAtIndex(idx).t);
  }
  irDef.functions.push_back(func);
}

auto getOpcodeFromString(Lexer::TokenType t, bool isReg) -> uint32_t {
  switch (t) {
  case Lexer::TokenType::Plus:
    return isReg ? 0x1 : 0x2;
  case Lexer::TokenType::Equal:
    return isReg ? 0x3 : 0x4;
  case Lexer::TokenType::Minus:
    return isReg ? 0x5 : 0x6;
  default:
    return 0;
  }
}

auto SimpleIRPass::emitExpression(const Parser &p, const AST::AstNode &node,
                                  IRListing &ir, const GlobalTable &gt,
                                  LocalTable &lt, TempReg dest) -> void {
  std::visit(
      overloaded{
          [&](const AST::Identifier &ident) {
            // We look at the local table first (scoping)
            if (lt.namedVars.contains(ident.name)) {
              IROp moveOp{};
              moveOp.destination = {dest, true};
              moveOp.opcode = 0x3;
              moveOp.source2 = {lt.namedVars[ident.name].tr, true};
              moveOp.s2type = Source2Type::Immediate;
              ir.push_back(moveOp);
            } else if (gt.contains(ident.name)) {
              emitGlobalNameBase(ir, gt, lt);
            }
          },
          [&](const AST::IntLiteral &intLiteral) {
            IROp moveOp{};
            moveOp.destination = {dest, true};
            moveOp.opcode = 0x4;
            moveOp.source2 = {static_cast<uint32_t>(intLiteral.literal), false};
            moveOp.s2type = Source2Type::Immediate;
            ir.push_back(moveOp);
          },
          [&](const AST::BinaryOperator &binaryOp) {
            auto lhsReg = lt.allocateAnonymous();
            emitExpression(p, p.getNodeAtIndex(binaryOp.lhsOp), ir, gt, lt,
                           lhsReg);

            if (std::holds_alternative<AST::IntLiteral>(
                    p.getNodeAtIndex(binaryOp.rhsOp).t)) {
              IROp op{};
              op.opcode = getOpcodeFromString(binaryOp.type, false);
              op.source1 = {lhsReg, true};
              op.source2 = {
                  static_cast<uint32_t>(std::get<AST::IntLiteral>(
                                            p.getNodeAtIndex(binaryOp.rhsOp).t)
                                            .literal),
                  false};
              op.destination = {dest, true};
              op.s2type = Immediate;
              ir.push_back(op);
            } else {
              auto rhsReg = lt.allocateAnonymous();
              emitExpression(p, p.getNodeAtIndex(binaryOp.rhsOp), ir, gt, lt,
                             rhsReg);

              IROp op{};
              op.opcode = getOpcodeFromString(binaryOp.type, true);
              op.source1 = {lhsReg, true};
              op.source2 = {rhsReg, true};
              op.destination = {dest, true};
              op.s2type = Register;
              ir.push_back(op);
            }
          },
          [&](const auto &other) {},
      },
      node.t);
}

auto SimpleIRPass::emitVariableIR(const Parser &p,
                                  const AST::VariableDecl &vdec, IRListing &ir,
                                  const GlobalTable &gt, LocalTable &lt)
    -> void {
  auto type = vdec.type;
  auto varTReg = lt.allocateNamed(vdec.name, vdec.type);

  const auto &defineNode = p.getNodeAtIndex(vdec.defineNode);
  emitExpression(p, defineNode, ir, gt, lt, varTReg);
}

auto SimpleIRPass::emitGlobalNameBase(IRListing &ir, const GlobalTable &gt,
                                      LocalTable &lt) -> TempReg {
  IROp moveOp{};
  moveOp.opcode = 0x4; // MovImm;
  moveOp.destination = {lt.allocateAnonymous(), true};
  moveOp.source1 = {0, true}; // TODO do gt base
  moveOp.s2type = Source2Type::Immediate;
  ir.push_back(moveOp);
  return moveOp.destination.val;
}

auto SimpleIRPass::emitBinaryOpIR(const AST::BinaryOperator &bOp, IRListing &ir,
                                  LocalTable &lt) -> void {}

auto calculateOffset(const std::string_view &type) -> std::uint32_t {
  if (type == "int")
    return 4;
  if (type == "long")
    return 8; // not sure how I'll work this
  if (type == "float")
    return 8;
  if (type == "string")
    return 4; // Will have to be an object, so I'll assume pointer type
  return 0;
}

auto SimpleIRPass::emitFunctionPrelude(const Parser &p,
                                       const AST::FunctionDef &funcDef,
                                       FunctionIR &ir, LocalTable &lt) -> void {
  uint32_t offset = 8; // RET + PREVIOUS SP
  for (const auto &paramId : funcDef.params) {
    const auto param =
        std::get<AST::FunctionParamDef>(p.getNodeAtIndex(paramId).t);
    auto vReg = lt.allocateNamed(param.name, param.type);
    ir.params[param.name] = {param.name, {vReg, true}, offset};
    IROp load{};
    load.opcode = 0x9; // LOAD
    load.destination = {vReg, true};
    load.source1 = {15, false}; // R15
    load.source2 = {offset, false};
    load.s2type = Immediate;
    ir.insts.push_back(load);
    offset += calculateOffset(param.type);
  }

  // LOAD
}
} // namespace Fanta
