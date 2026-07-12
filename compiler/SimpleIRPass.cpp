#include "SimpleIRPass.hpp"
#include "ast.hpp"
#include "cpu_info.hpp"
#include "ir.hpp"
#include <variant>

namespace Fanta {

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

auto SimpleIRPass::outputIR(Parser &p, GlobalTable &gt) -> IR {
  IR ir{};

  FunctionIR initFunc{};
  initFunc.name = "__init";
  initFunc.calleeRegs = {};
  for (const auto &idx : p.getRootIndices()) {
    const auto &cr = p.getNodeAtIndex(idx);
    std::visit(overloaded{[&](const AST::VariableDecl &vdec) {
                            emitGlobalVariableIR(p, vdec, initFunc, gt);
                          },
                          [&](const AST::FunctionDef &fdef) {
                            emitFunctionDef(p, fdef, ir, gt);
                          },
                          [&](const auto &other) {}},
               cr.t);
  }
  ir.functions.insert(ir.functions.begin(), initFunc);
  return ir;
}

auto SimpleIRPass::emitGlobalVariableIR(const Parser &p,
                                        const AST::VariableDecl &decl,
                                        FunctionIR &ir, GlobalTable &gt)
    -> void {
  auto &varName = decl.name;
  GlobalVarInfo info{};
  LocalTable lt{std::string(varName)};
  auto dest = lt.allocateAnonymous();
  if (gt.contains(varName)) {
    info = std::get<GlobalVarInfo>(gt[varName]);
  } else {
    info.rt = decl.type;
    info.offsetFromBase = globalOffsets;
    globalOffsets += 4;
    gt[varName] = info;
  }

  emitExpression(p, p.getNodeAtIndex(decl.defineNode), ir.insts, gt, lt, dest);

  auto baseReg = emitGlobalNameBase(ir.insts, gt, lt);
  IROp store{};
  store.opcode = Info::Instructions::STORE;
  store.destination = {dest, true};
  store.source1 = {baseReg, false};
  store.source2 = {info.offsetFromBase, false};
  ir.insts.push_back(store);
}

auto SimpleIRPass::emitFunctionDef(const Parser &p,
                                   const AST::FunctionDef &fdef, IR &irDef,
                                   const GlobalTable &gt) -> void {
  const auto bodyNode =
      std::get<AST::FunctionBody>(p.getNodeAtIndex(fdef.body).t);

  LocalTable lt{std::string(fdef.name)};
  FunctionIR func{};
  auto &ir = func.insts;

  func.name = fdef.name;
  emitFunctionPrelude(p, fdef, func, lt);
  for (const auto &idx : bodyNode.expressions) {
    emitStatement(p, p.getNodeAtIndex(idx), ir, gt, lt);
  }
  irDef.functions.push_back(func);
}

// Lowers a single function-body-level statement. Shared by emitFunctionDef's
// top-level body and emitIf's body/else blocks so every statement kind is
// only handled in one place.
auto SimpleIRPass::emitStatement(const Parser &p, const AST::AstNode &node,
                                 IRListing &ir, const GlobalTable &gt,
                                 LocalTable &lt) -> void {
  std::visit(
      overloaded{
          [&](const AST::BinaryOperator &bOp) { emitBinaryOpIR(bOp, ir, lt); },
          [&](const AST::VariableDecl &vdec) {
            emitVariableIR(p, vdec, ir, gt, lt);
          },
          [&](const AST::ReturnVal &rval) {
            emitReturnIR(p, rval, ir, gt, lt);
          },
          [&](const AST::FunctionCall &fcall) {
            emitCall(p, fcall, ir, gt, lt);
          },
          [&](const AST::IfStm &ifStm) { emitIf(p, ifStm, ir, gt, lt); },
          [&](const auto &other) {}},
      node.t);
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

auto SimpleIRPass::emitComparison(const Parser &p,
                                  const AST::BinaryOperator &bcall,
                                  IRListing &ir, const GlobalTable &gt,
                                  LocalTable &lt) -> void {
  auto lhsVal = lt.allocateAnonymous();
  emitExpression(p, p.getNodeAtIndex(bcall.lhsOp), ir, gt, lt, lhsVal);
  auto rhsVal = lt.allocateAnonymous();
  emitExpression(p, p.getNodeAtIndex(bcall.rhsOp), ir, gt, lt, rhsVal);
  IROp cmp{};
  cmp.opcode = Info::Instructions::CMP_REG;
  cmp.destination = {lhsVal, true}; // CMP has no dest
  cmp.source2 = {rhsVal, true};
  cmp.s2type = Register;
  ir.push_back(cmp);
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
              auto baseReg = emitGlobalNameBase(ir, gt, lt);
              IROp loadGlobal{};
              loadGlobal.opcode = Info::Instructions::LOAD;
              loadGlobal.destination = {dest, true};
              loadGlobal.source1 = {baseReg, true};
              loadGlobal.source2 = {
                  std::get<GlobalVarInfo>(gt.at(ident.name)).offsetFromBase,
                  false};
              loadGlobal.s2type = Source2Type::Immediate;
              ir.push_back(loadGlobal);
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
          [&](const AST::FunctionCall &fcall) {
            CallFunc cf;
            cf.dest = {dest, true};
            cf.name =
                std::get<AST::Identifier>(p.getNodeAtIndex(fcall.funcNameIdx).t)
                    .name;
            for (auto it = fcall.params.rbegin(); it != fcall.params.rend();
                 ++it) {
              auto expr = lt.allocateAnonymous();
              emitExpression(p, p.getNodeAtIndex(*it), ir, gt, lt, expr);
              IROp op{};
              op.opcode = Info::Instructions::PUSH;
              op.destination = {expr, true};
              ir.push_back(op);
            }
            ir.push_back(cf);
            // Restore stack pointer at the end
            if (!fcall.params.empty()) {
              IROp restoreStack{};
              restoreStack.opcode = Info::Instructions::ADD_IMM;
              restoreStack.source2 = {
                  static_cast<uint32_t>(4 * fcall.params.size()), false};
              restoreStack.source1 = {Info::Registers::SP, false};
              restoreStack.destination = {Info::Registers::SP, false};
              restoreStack.s2type = Immediate;
              ir.push_back(restoreStack);
            }
          },
          [&](const auto &other) {},
      },
      node.t);
}

auto generateAnd() -> void {}

auto generateOr() -> void {}

auto isComparator(const Lexer::TokenType ttype) -> bool {
  return ttype == Lexer::TokenType::Greater ||
         ttype == Lexer::TokenType::Lesser ||
         ttype == Lexer::TokenType::GreaterEq ||
         ttype == Lexer::TokenType::EqualComp ||
         ttype == Lexer::TokenType::NotEq ||
         ttype == Lexer::TokenType::LesserEq;
}

auto generatePassJump(IRListing &ir, const Lexer::TokenType compType,
                      std::string &jmpLabel) -> void {
  switch (compType) {
  case Lexer::TokenType::Greater: {
    Branch op{};
    op.opcode = Info::Instructions::BEC;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::Lesser: {
    Branch op{};
    op.opcode = Info::Instructions::BMI;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::GreaterEq: {
    Branch op{};
    op.opcode = Info::Instructions::BPL;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::EqualComp: {
    Branch op{};
    op.opcode = Info::Instructions::BEQ;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::NotEq: {
    Branch op{};
    op.opcode = Info::Instructions::BNE;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::LesserEq: {
    Branch op{};
    op.opcode = Info::Instructions::BNC;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  default:
    return;
  }
}

auto generateFailJump(IRListing &ir, const Lexer::TokenType compType,
                      std::string &jmpLabel) -> void {
  switch (compType) {
  case Lexer::TokenType::Greater: {
    Branch op{};
    op.opcode = Info::Instructions::BNC;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::Lesser: {
    Branch op{};
    op.opcode = Info::Instructions::BPL;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::GreaterEq: {
    Branch op{};
    op.opcode = Info::Instructions::BMI;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::EqualComp: {
    Branch op{};
    op.opcode = Info::Instructions::BNE;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::NotEq: {
    Branch op{};
    op.opcode = Info::Instructions::BEQ;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  case Lexer::TokenType::LesserEq: {
    Branch op{};
    op.opcode = Info::Instructions::BEC;
    op.label = jmpLabel;
    ir.push_back(op);
    break;
  }
  default:
    return;
  }
}

auto SimpleIRPass::handleLogicalCheck(const Parser &p,
                                      const AST::BinaryOperator &bop,
                                      IRListing &ir, const GlobalTable &gt,
                                      LocalTable &lt, Lexer::TokenType type,
                                      std::string exitLabel,
                                      std::string earlyEntry) -> void {
  switch (type) {
  case Lexer::TokenType::And: {
    emitCondFalse(p, p.getNodeAtIndex(bop.lhsOp), ir, gt, lt, exitLabel);
    emitCondTrue(p, p.getNodeAtIndex(bop.rhsOp), ir, gt, lt, earlyEntry);
    Branch jmp{};
    jmp.opcode = Info::Instructions::JREL;
    jmp.label = exitLabel;
    ir.push_back(jmp);
    break;
  }
  case Lexer::TokenType::Or: {
    emitCondTrue(p, p.getNodeAtIndex(bop.lhsOp), ir, gt, lt, earlyEntry);
    emitCondFalse(p, p.getNodeAtIndex(bop.rhsOp), ir, gt, lt, exitLabel);
    break;
  }
  default:
    break; // Blow up eventually, how did we get here
  }
}

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
auto SimpleIRPass::handleNestedCondTrueLogicalCheck(
    const Parser &p, const AST::BinaryOperator &bop, IRListing &ir,
    const GlobalTable &gt, LocalTable &lt, Lexer::TokenType type,
    std::string happyPath) -> void {
  switch (type) {
  case Lexer::TokenType::And: {
    auto skipExpression = lt.generateNewLabel();
    emitCondFalse(p, p.getNodeAtIndex(bop.lhsOp), ir, gt, lt, skipExpression);
    emitCondTrue(p, p.getNodeAtIndex(bop.rhsOp), ir, gt, lt, happyPath);
    ir.push_back(IRLabel{skipExpression});
    break;
  }
  case Lexer::TokenType::Or: {
    emitCondTrue(p, p.getNodeAtIndex(bop.lhsOp), ir, gt, lt, happyPath);
    emitCondTrue(p, p.getNodeAtIndex(bop.rhsOp), ir, gt, lt, happyPath);
    break;
  }
  default:
    break; // Blow up eventually, how did we get here
  }
}

auto SimpleIRPass::emitCondTrue(const Parser &p, const AST::AstNode &node,
                                IRListing &ir, const GlobalTable &gt,
                                LocalTable &lt, std::string earlyEntry)
    -> void {
  std::visit(overloaded{[&](const AST::BinaryOperator &bop) {
                          auto ttype = bop.type;
                          if (isComparator(ttype)) {
                            emitComparison(p, bop, ir, gt, lt);
                            generatePassJump(ir, ttype, earlyEntry);
                          } else {
                            handleNestedCondTrueLogicalCheck(p, bop, ir, gt, lt,
                                                             ttype, earlyEntry);
                          }
                        },
                        [&](const auto &other) {}},
             node.t);
};

auto SimpleIRPass::emitCondFalse(const Parser &p, const AST::AstNode &node,
                                 IRListing &ir, const GlobalTable &gt,
                                 LocalTable &lt, std::string jumpLabel)
    -> void {
  auto localSkip = lt.generateNewLabel();

  std::visit(overloaded{[&](const AST::BinaryOperator &bop) {
                          auto ttype = bop.type;
                          if (isComparator(ttype)) {
                            emitComparison(p, bop, ir, gt, lt);
                            generateFailJump(ir, ttype, jumpLabel);
                          } else {
                            handleLogicalCheck(p, bop, ir, gt, lt, ttype,
                                               jumpLabel, localSkip);
                          }
                        },
                        [&](const auto &other) {}},
             node.t);
  ir.push_back(IRLabel{localSkip});
};

auto SimpleIRPass::emitConditionalBranch(const Parser &p,
                                         const AST::BinaryOperator &bcall,
                                         IRListing &ir, const GlobalTable &gt,
                                         LocalTable &lt, std::string jumpLabel)
    -> void {
  auto ttype = bcall.type;
  emitComparison(p, bcall, ir, gt, lt);
  generateFailJump(ir, ttype, jumpLabel);
};

auto SimpleIRPass::emitIf(const Parser &p, const AST::IfStm &ifStm,
                          IRListing &ir, const GlobalTable &gt, LocalTable &lt)
    -> void {
  auto jumpLabel = lt.generateNewLabel();
  auto jumpPastEl = lt.generateNewLabel();
  auto jumpIntoIf = lt.generateNewLabel();
  auto el = ifStm.elbody;
  std::visit(overloaded{
                 [&](const AST::BinaryOperator &bcall) {
                   // the top token defines our branch type
                   auto ttype = bcall.type;
                   if (isComparator(ttype)) {
                     emitConditionalBranch(p, bcall, ir, gt, lt, jumpLabel);
                   } else {
                     handleLogicalCheck(p, bcall, ir, gt, lt, ttype, jumpLabel,
                                        jumpIntoIf);
                   }
                 },
                 [&](const auto &other) {},
             },
             p.getNodeAtIndex(ifStm.cond).t);
  // PUSH BACK JUMP WITH LABEL

  auto body = std::get<AST::FunctionBody>(p.getNodeAtIndex(ifStm.body).t);

  ir.push_back(IRLabel{jumpIntoIf});
  for (const auto &idx : body.expressions) {
    emitStatement(p, p.getNodeAtIndex(idx), ir, gt, lt);
  }

  bool hasReturn = std::holds_alternative<Return>(ir.back());

  if (!hasReturn && el) {
    Branch jmp{};
    jmp.opcode = Info::Instructions::JREL;
    jmp.label = jumpPastEl;
    ir.push_back(jmp);
  }

  // emit body emit(p, body, ir, gt, lt);
  ir.push_back(IRLabel{jumpLabel});
  if (el) {
    auto elBody = std::get<AST::FunctionBody>(p.getNodeAtIndex(*el).t);
    for (const auto &idx : elBody.expressions) {
      emitStatement(p, p.getNodeAtIndex(idx), ir, gt, lt);
    }
  }

  if (!hasReturn && el) {
    ir.push_back(IRLabel{jumpPastEl});
  }
}

auto SimpleIRPass::emitCall(const Parser &p, const AST::FunctionCall &fcall,
                            IRListing &ir, const GlobalTable &gt,
                            LocalTable &lt) -> void {
  CallFunc cf;
  cf.name =
      std::get<AST::Identifier>(p.getNodeAtIndex(fcall.funcNameIdx).t).name;
  for (auto it = fcall.params.rbegin(); it != fcall.params.rend(); ++it) {
    auto expr = lt.allocateAnonymous();
    emitExpression(p, p.getNodeAtIndex(*it), ir, gt, lt, expr);
    IROp op{};
    op.opcode = Info::Instructions::PUSH;
    op.destination = {expr, true};
    ir.push_back(op);
  }
  ir.push_back(cf);
  // Restore stack pointer at the end
  if (!fcall.params.empty()) {
    IROp restoreStack{};
    restoreStack.opcode = Info::Instructions::ADD_IMM;
    restoreStack.source2 = {static_cast<uint32_t>(4 * fcall.params.size()),
                            false};
    restoreStack.source1 = {Info::Registers::SP, false};
    restoreStack.destination = {Info::Registers::SP, false};
    restoreStack.s2type = Immediate;
    ir.push_back(restoreStack);
  }
}

// Computes the return value into R0, then pushes a Return marker so
// instruction_emit generates the epilogue+RET right here rather than falling
// through to whatever statement comes next.
auto SimpleIRPass::emitReturnIR(const Parser &p, const AST::ReturnVal &vdec,
                                IRListing &ir, const GlobalTable &gt,
                                LocalTable &lt) -> void {
  auto varTReg = lt.allocateAnonymous();
  const auto &defineNode = p.getNodeAtIndex(vdec.returnVal);
  emitExpression(p, defineNode, ir, gt, lt, varTReg);
  IROp movRetToR0{};
  movRetToR0.destination = {0, false};
  movRetToR0.source2 = {varTReg, true};
  movRetToR0.s2type = Register;
  movRetToR0.opcode = Info::Instructions::MOV_REG;
  ir.push_back(movRetToR0);
  ir.push_back(Return{});
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
  LocalGlobalBase lgb{};
  lgb.dest = {lt.allocateAnonymous(), true};
  ir.push_back(lgb);
  return lgb.dest.val;
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
