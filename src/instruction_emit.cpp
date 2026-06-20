#include "instruction_emit.hpp"
#include "PassDefinition.hpp"
#include "cpu_info.hpp"
#include "instructions.hpp"
#include "ir.hpp"
#include <ranges>
#include <variant>

namespace Fanta {
auto InstructionEmitter::outputInstructions(RegAllocIR &rir, GlobalTable &gt)
    -> InstructionList {
  InstructionList list{};

  // Emit the actual instructions within the instruction
  for (const auto &func : rir.functions) {
    InstructionList funcList{}; // this is just so I can track where the start
                                // of each func is
    outputInstructionsForFunc(func, gt, funcList);
    auto baseAddr = list.size();
    list.reserve(list.size() + funcList.size());
    list.append_range(funcList);
    resolvedAddresses[func.name] = baseAddr;
  }

  link(list);

  return list;
}

auto InstructionEmitter::link(InstructionList &il) -> void {
  for (const auto &[funcName, baseAddr] : resolvedAddresses) {
    if (missingLinks.contains(funcName)) {
      const auto &list = missingLinks[funcName];
      for (const auto &[idx, linkName] : list) {
        if (!resolvedAddresses.contains(linkName)) {
        } // BLOW UP
        const auto &func = il[baseAddr + idx];
        if ((func >> 26 & 0xFF) == 0x15) {
          auto newAddr = (resolvedAddresses[linkName] - (baseAddr + idx)) * 4;
          auto newFunc = Instructions::Emitter::single_inst(0x15, newAddr);
          il[baseAddr + idx] = newFunc;
        } // it's a call
      }
    }
  }
}

auto InstructionEmitter::outputInstructionsForFunc(const FunctionIR &fir,
                                                   GlobalTable &gt,
                                                   InstructionList &il)
    -> void {

  auto maxOffset = fir.localVarCount;

  il.push_back(Instructions::Emitter::single_inst(Info::Instructions::PUSH,
                                                  Info::Registers::FP));

  // First save caller registers into the stack, we do 0 -> 14
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
                     missingLinks[fir.name].push_back({il.size() - 1, op.name});
                   },
                   [](const auto &other) {}},
        ir);
  }

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
  // Call ret (TODO: Currently Ret doesn't push values or anything so we should
  // have to do some work to push values to R0)
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
  }
}
} // namespace Fanta
