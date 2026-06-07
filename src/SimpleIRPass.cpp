#include "SimpleIRPass.hpp"
#include "ast.hpp"
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
  const auto &bodyNode =
      std::get<AST::FunctionBody>(p.getNodeAtIndex(fdef.body).t);

  LocalTable lt{};
  FunctionIR func{};
  auto &ir = func.insts;

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
}

auto SimpleIRPass::emitVariableIR(const Parser &p,
                                  const AST::VariableDecl &vdec, IRListing &ir,
                                  const GlobalTable &gt, LocalTable &lt)
    -> void {
  auto type = vdec.type;
  auto varTReg = lt.allocateNamed(vdec.name, vdec.type);

  const auto &defineNode = p.getNodeAtIndex(vdec.defineNode);

  std::visit(overloaded{
                 [&](const AST::Identifier &ident) {},
                 [&](const AST::IntLiteral &intLiteral) {},
                 [&](const auto &other) {},
             },
             defineNode.t);
}

auto SimpleIRPass::emitGlobalNameBase(IRListing &ir, const GlobalTable &gt,
                                      LocalTable &lt) -> TempReg {
  IROp moveOp{};
  moveOp.opcode = 0x4; // MovImm;
  moveOp.destination = lt.allocateAnonymous();
  moveOp.source1 = 0; // TODO do gt base
  moveOp.s2type = Source2Type::Immediate;
  ir.push_back(moveOp);
  return moveOp.destination;
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
    const auto &param =
        std::get<AST::FunctionParamDef>(p.getNodeAtIndex(paramId).t);
    ir.params[param.name] = {param.name, lt.allocateAnonymous(), offset};
    offset += calculateOffset(param.type);
  }
}
} // namespace Fanta
