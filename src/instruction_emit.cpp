#include "instruction_emit.hpp"
#include "PassDefinition.hpp"
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

  il.push_back(Instructions::Emitter::single_inst(0x1D, 15));

  // First save caller registers into the stack, we do 0 -> 14
  for (const auto &reg : fir.calleeRegs) {
    il.push_back(Instructions::Emitter::single_inst(0x1D, reg.val));
  }

  // Move our current frame-pointer down to allocate all local vars
  if (maxOffset > 0) {
    il.push_back(Instructions::Emitter::three_op_imm(0x6, 16, 16, maxOffset));
  }
  il.push_back(Instructions::Emitter::two_op(0x3, 15, 16));

  for (auto &ir : fir.insts) {
    std::visit(
        overloaded{[&](const IROp &op) { emitInst(op, gt, il); },
                   [&](const CallFunc &op) {
                     il.push_back(Instructions::Emitter::single_inst(
                         0x15, -1)); // We probably don't know the callsite yet.
                     missingLinks[fir.name].push_back({il.size() - 1, op.name});
                   },
                   [](const auto &other) {}},
        ir);
  }

  // This sets us back to before any allocated vars in the stack pointer
  if (maxOffset > 0) {
    il.push_back(Instructions::Emitter::three_op_imm(0x2, 16, 16, maxOffset));
  }

  // Now issue the pop of registers
  for (const auto &reg : std::views::reverse(fir.calleeRegs)) {
    il.push_back(Instructions::Emitter::single_inst(0x1E, reg.val));
  }
  // Pop the caller FP
  il.push_back(Instructions::Emitter::single_inst(0x1E, 15));
  // Call ret (TODO: Currently Ret doesn't push values or anything so we should
  // have to do some work to push values to R0)
  il.push_back(Instructions::Emitter::single_inst(0x16, 0));
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
    SINGLE(0)
    THREE_REG(0x1)
    THREE_IMM(0x2)
    TWO_REG(0x3)
    TWO_IMM(0x4)
    THREE_REG(0x5)
    THREE_IMM(0x6)
    SINGLE(0x7)
    THREE_IMM(0x8)
    THREE_IMM(0x9)
    SINGLE(0xa)
    SINGLE(0xb)
    THREE_REG(0xc)
    THREE_IMM(0xd)
    TWO_REG(0xe)
    TWO_IMM(0xf)
    SINGLE(0x10)
    SINGLE(0x11)
    SINGLE(0x12)
    SINGLE(0x13)
    SINGLE(0x14)
    SINGLE(0x15)
    SINGLE(0x16)
    THREE_REG(0x17)
    THREE_IMM(0x18)
    THREE_REG(0x19)
    THREE_IMM(0x1A)
    THREE_REG(0x1B)
    THREE_IMM(0x1C)
    SINGLE(0x1D)
    SINGLE(0x1E)
  }
}
} // namespace Fanta
