#include "allocator.hpp"
#include "ir.hpp"

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Fanta {
auto Allocator::assignFunc(const FunctionIR &function) -> FunctionIR {
  reset();
  FunctionIR fir = function;
  fir.insts.clear();
  for (auto &ir : function.insts) {
    std::visit(
        overloaded{[&](const IROp &op) {
                     IROp assignedOp = op;
                     if (op.source1.isVirtual) {
                       assignedOp.source1.val =
                           getNextAvailable(fir, op.source1.val, StoreAndLoad);
                       assignedOp.source1.isVirtual = false;
                     }
                     if (op.source2.isVirtual) {
                       assignedOp.source2.val =
                           getNextAvailable(fir, op.source2.val, StoreAndLoad);
                       assignedOp.source2.isVirtual = false;
                     }
                     if (op.destination.isVirtual) {
                       assignedOp.destination.val =
                           getNextAvailable(fir, op.destination.val, Store);
                       assignedOp.destination.isVirtual = false;
                     }
                     fir.insts.push_back(assignedOp);
                   },
                   [](const auto &other) {}},
        ir);
  }

  for (uint32_t i = 0; i < usedRegisters.size(); i++) {
    if (usedRegisters[i]) {
      fir.calleeRegs.push_back({i, false});
    }
  }
  fir.localVarCount = offset; // reusing this varcount to know how big our frame
                              // pointer offset will be

  auto spillSize = fir.calleeRegs.size() * 4;
  auto adjustment = offset + spillSize + 4;

  for (auto i = 0; i < std::min(fir.calleeRegs.size(), fir.insts.size()); i++) {
    std::visit(overloaded{[&](IROp &op) {
                            if (op.opcode == 0x9 && op.source1.val == 15 &&
                                op.s2type == Immediate) {
                              op.source2.val += adjustment;
                            }
                          },
                          [](const auto &other) {}},
               fir.insts[i]);
  }

  return fir;
}

/**
 * check if it's not been allocated, if so return it
 * if we run out of registers, take the current one (if it's not we need to
 * spill but make sure it's not one of the currently used)
 */
auto Allocator::getNextAvailable(FunctionIR &func, uint32_t reg,
                                 SpillMechanism smech) -> std::uint16_t {
  // We had previously allocated the register
  if (virtToReg.contains(reg)) {
    // It's still assigned
    if (regToVirt.contains(virtToReg[reg]) && regToVirt[virtToReg[reg]] == reg)
      return virtToReg[reg];
    else {
      // It's no longer assgined so we should make sure to store whatever that
      // register is currently and load the vreg
      stashAndLoad(func, reg, smech);
      regToVirt[virtToReg[reg]] = reg;
      return virtToReg[reg];
    }
  }
  if (usedRegisters[next]) {
    virtToReg[reg] = next;
    stashAndLoad(func, reg, smech);
  }

  auto allocatedReg = next;
  next = (next + 1) % 15;
  virtToReg[reg] = allocatedReg;
  regToVirt[allocatedReg] = reg;
  virtToStackOffset[reg] = offset + 4;
  offset += 4;
  usedRegisters[allocatedReg] = true;
  return allocatedReg;
}

// Dump currently used version of register
auto Allocator::stashAndLoad(FunctionIR &func, uint32_t reg,
                             SpillMechanism smech) -> void {
  uint32_t preg = virtToReg[reg];
  if (regToVirt.contains(preg)) {
    uint32_t evictedVirt = regToVirt[preg];
    emitStore(func, evictedVirt, preg);
    regToVirt.erase(preg);
  }
  if (smech == SpillMechanism::StoreAndLoad) {
    emitLoad(func, reg, preg);
  }
}

auto Allocator::emitLoad(FunctionIR &func, uint32_t vreg, uint32_t preg)
    -> void {
  IROp load{};
  load.opcode = 0x9;
  load.destination = {preg, false};
  load.source1 = {15, false};
  load.source2 = {virtToStackOffset[vreg], false};
  load.s2type = Immediate;
  func.insts.push_back(load);
}

auto Allocator::emitStore(FunctionIR &func, uint32_t vreg, uint32_t preg)
    -> void {
  IROp store{};
  store.opcode = 0x8;
  store.destination = {preg, false};
  store.source1 = {15, false};
  store.source2 = {virtToStackOffset[vreg], false};
  store.s2type = Immediate;
  func.insts.push_back(store);
}
} // namespace Fanta
