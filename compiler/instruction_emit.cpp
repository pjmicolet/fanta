#include "instruction_emit.hpp"
#include "PassDefinition.hpp"
#include "cpu_info.hpp"
#include "instructions.hpp"
#include "ir.hpp"
#include <ranges>
#include <string>
#include <fanta_utils.hpp>
#include <variant>

namespace Fanta {

auto InstructionEmitter::outputInstructions(RegAllocIR &rir, GlobalTable &gt)
    -> InstructionList {
  InstructionList list{};

  reset();

  // Emit the actual instructions within the instruction
  for (const auto &func : rir.functions) {
    auto baseAddr = list.size();
    resolvedAddresses[func.name] = baseAddr;
    if (func.name == "__init") {
      outputInitFunc(func, gt, list);
    } else {
      outputInstructionsForFunc(func, gt, list);
    }
  }

  link(list);

  return list;
}

auto InstructionEmitter::link(InstructionList &il) -> void {
  auto globalBaseAddr = ((il.size() * 4 + 15) / 16) * 16;

  for (const auto &movId : globalBaseMovs) {
    auto reg = (il[movId] >> 21) & 0x1F;
    il[movId] = Instructions::Emitter::two_op_imm(Info::Instructions::MOV_IMM,
                                                  reg, globalBaseAddr);
  }
  for (const auto &[idx, linkName] : missingLinks) {
    if (!resolvedAddresses.contains(linkName)) {
      throw std::runtime_error("Unknown symbol: " + std::string{linkName});
    }
    const auto &func = il[idx];
    const auto opcode = func >> 26 & 0xFF;
    if ((opcode == Info::Instructions::CALL) ||
        (opcode == Info::Instructions::BNC) ||
        (opcode == Info::Instructions::BEQ) ||
        (opcode == Info::Instructions::BEC) ||
        (opcode == Info::Instructions::BNE) ||
        (opcode == Info::Instructions::BPL) ||
        (opcode == Info::Instructions::BMI) ||
        (opcode == Info::Instructions::JREL) ||
        (opcode == Info::Instructions::JUMP)) {
      auto newAddr = (resolvedAddresses[linkName] - (idx)) * 4;
      auto newFunc = Instructions::Emitter::single_inst(opcode, newAddr);
      il[idx] = newFunc;
    } // it's a call
  }
}

auto InstructionEmitter::outputInitFunc(const FunctionIR &fir, GlobalTable &gt,
                                        InstructionList &il) -> void {

  for (auto &ir : fir.insts) {
    std::visit(
        overloaded{[&](const IROp &op) { emitInst(op, gt, il); },
                   [&](const CallFunc &op) {
                     il.push_back(Instructions::Emitter::single_inst(
                         Info::Instructions::CALL,
                         -1)); // We probably don't know the callsite yet.
                     missingLinks.push_back({il.size() - 1, op.name});
                     if (op.dest) {
                       il.push_back(Instructions::Emitter::two_op(
                           Info::Instructions::MOV_REG, op.dest->val, 0));
                     }
                   },
                   [&](const LocalGlobalBase &lgb) {
                     auto movGlobal = Instructions::Emitter::two_op_imm(
                         Info::Instructions::MOV_IMM, lgb.dest.val, 0);
                     il.push_back(movGlobal);
                     globalBaseMovs.push_back(il.size() - 1);
                   },
                   [](const auto &other) {}},
        ir);
  }

  il.push_back(
      Instructions::Emitter::single_inst(Info::Instructions::CALL, -1));
  missingLinks.push_back({il.size() - 1, "main"});
  il.push_back(
      Instructions::Emitter::single_inst(Info::Instructions::HALT, -1));
}

auto InstructionEmitter::outputInstructionsForFunc(const FunctionIR &fir,
                                                   GlobalTable &gt,
                                                   InstructionList &il)
    -> void {

  auto maxOffset = fir.localVarCount;

  il.push_back(Instructions::Emitter::single_inst(Info::Instructions::PUSH,
                                                  Info::Registers::FP));

  // First save caller registers into the stack, we do 1 -> 14
  for (const auto &reg : fir.calleeRegs) {
    il.push_back(
        Instructions::Emitter::single_inst(Info::Instructions::PUSH, reg.val));
  }

  // Move our current frame-pointer down to allocate all local vars
  if (maxOffset > 0) {
    il.push_back(Instructions::Emitter::three_op_imm(
        Info::Instructions::SUB_IMM, Info::Registers::SP, Info::Registers::SP,
        maxOffset));
  }
  il.push_back(Instructions::Emitter::two_op(
      Info::Instructions::MOV_REG, Info::Registers::FP, Info::Registers::SP));

  for (auto &ir : fir.insts) {
    std::visit(
        overloaded{[&](const IROp &op) { emitInst(op, gt, il); },
                   [&](const CallFunc &op) {
                     il.push_back(Instructions::Emitter::single_inst(
                         Info::Instructions::CALL,
                         -1)); // We probably don't know the callsite yet.
                     missingLinks.push_back({il.size() - 1, op.name});
                     if (op.dest) {
                       il.push_back(Instructions::Emitter::two_op(
                           Info::Instructions::MOV_REG, op.dest->val, 0));
                     }
                   },
                   [&](const Branch &bop) {
                     il.push_back(
                         Instructions::Emitter::single_inst(bop.opcode, -1));
                     missingLinks.push_back({il.size() - 1, bop.label});
                   },
                   [&](const Return &ret) { emitEpilogue(fir, maxOffset, il); },
                   [&](const LocalGlobalBase &lgb) {
                     auto movGlobal = Instructions::Emitter::two_op_imm(
                         Info::Instructions::MOV_IMM, lgb.dest.val, 0);
                     il.push_back(movGlobal);
                     globalBaseMovs.push_back(il.size() - 1);
                   },
                   [&](const IRLabel &label) {
                     resolvedAddresses[label.name] = il.size();
                   },
                   [](const auto &other) {}},
        ir);
  }

  // Falls off the end without an explicit return: still needs the epilogue.
  emitEpilogue(fir, maxOffset, il);
}

auto InstructionEmitter::emitEpilogue(const FunctionIR &fir, uint32_t maxOffset,
                                      InstructionList &il) -> void {
  // This sets us back to before any allocated vars in the stack pointer
  if (maxOffset > 0) {
    il.push_back(Instructions::Emitter::three_op_imm(
        Info::Instructions::ADD_IMM, Info::Registers::SP, Info::Registers::SP,
        maxOffset));
  }

  // Now issue the pop of registers
  for (const auto &reg : std::views::reverse(fir.calleeRegs)) {
    il.push_back(
        Instructions::Emitter::single_inst(Info::Instructions::POP, reg.val));
  }
  // Pop the caller FP
  il.push_back(Instructions::Emitter::single_inst(Info::Instructions::POP,
                                                  Info::Registers::FP));
  // Call ret (TODO: Currently Ret doesn't push values or anything so we
  // should have to do some work to push values to R0)
  il.push_back(Instructions::Emitter::single_inst(Info::Instructions::RET, 0));
}

auto InstructionEmitter::emitInst(const IROp &op, GlobalTable &gt,
                                  InstructionList &il) -> void {
#define THREE_REG(num)                                                         \
  case num: {                                                                  \
    il.push_back(Instructions::Emitter::three_op(                              \
        op.opcode, op.destination.val, op.source1.val, op.source2.val));       \
    break;                                                                     \
  }

#define THREE_IMM(num)                                                         \
  case num: {                                                                  \
    il.push_back(Instructions::Emitter::three_op_imm(                          \
        op.opcode, op.destination.val, op.source1.val, op.source2.val));       \
    break;                                                                     \
  }
#define SINGLE(num)                                                            \
  case num: {                                                                  \
    il.push_back(                                                              \
        Instructions::Emitter::single_inst(op.opcode, op.destination.val));    \
    break;                                                                     \
  }
#define TWO_REG(num)                                                           \
  case num: {                                                                  \
    il.push_back(Instructions::Emitter::two_op(op.opcode, op.destination.val,  \
                                               op.source2.val));               \
    break;                                                                     \
  }
#define TWO_IMM(num)                                                           \
  case num: {                                                                  \
    il.push_back(Instructions::Emitter::two_op_imm(                            \
        op.opcode, op.destination.val, op.source2.val));                       \
    break;                                                                     \
  }
  switch (op.opcode) {
    SINGLE(Info::Instructions::HALT)
    THREE_REG(Info::Instructions::ADD_REG)
    THREE_IMM(Info::Instructions::ADD_IMM)
    TWO_REG(Info::Instructions::MOV_REG)
    TWO_IMM(Info::Instructions::MOV_IMM)
    THREE_REG(Info::Instructions::SUB_REG)
    THREE_IMM(Info::Instructions::SUB_IMM)
    SINGLE(Info::Instructions::JUMP)
    THREE_IMM(Info::Instructions::STORE)
    THREE_IMM(Info::Instructions::LOAD)
    SINGLE(Info::Instructions::BEQ)
    SINGLE(Info::Instructions::BNE)
    THREE_REG(Info::Instructions::LSH_REG)
    THREE_IMM(Info::Instructions::LSH_IMM)
    TWO_REG(Info::Instructions::CMP_REG)
    TWO_IMM(Info::Instructions::CMP_IMM)
    SINGLE(Info::Instructions::BEC)
    SINGLE(Info::Instructions::BMI)
    SINGLE(Info::Instructions::BPL)
    SINGLE(Info::Instructions::JREL)
    SINGLE(Info::Instructions::NOP)
    SINGLE(Info::Instructions::CALL)
    SINGLE(Info::Instructions::RET)
    THREE_REG(Info::Instructions::AND_REG)
    THREE_IMM(Info::Instructions::AND_IMM)
    THREE_REG(Info::Instructions::OR_REG)
    THREE_IMM(Info::Instructions::OR_IMM)
    THREE_REG(Info::Instructions::XOR_REG)
    THREE_IMM(Info::Instructions::XOR_IMM)
    SINGLE(Info::Instructions::PUSH)
    SINGLE(Info::Instructions::POP)
    SINGLE(Info::Instructions::BNC)
  }
}

auto InstructionEmitter::reset() -> void {
  globalBaseMovs.clear();
  missingLinks.clear();
  resolvedAddresses.clear();
}
} // namespace Fanta
