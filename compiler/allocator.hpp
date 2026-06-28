#pragma once

#include <ir.hpp>
#include <vector>

namespace Fanta {

struct Allocator {
  Allocator() : usedRegisters(15, false), next(0) {}

  auto assignFunc(const FunctionIR &function) -> FunctionIR;

private:
  auto reset() -> void {
    next = 1;
    for (int i = 0; i < usedRegisters.size(); i++)
      usedRegisters[i] = false;
    virtToReg.clear();
    regToVirt.clear();
    virtToStackOffset.clear();
    offset = 0;
  }

  enum SpillMechanism {
    Store,       // For when you need to assign the register to a destination
    StoreAndLoad // For when you are assigning sources and need to dump
  };

  auto emitStore(FunctionIR &func, uint32_t vreg, uint32_t preg) -> void;
  auto emitLoad(FunctionIR &func, uint32_t vreg, uint32_t preg) -> void;
  auto getNextAvailable(FunctionIR &func, uint32_t reg, SpillMechanism smech)
      -> std::uint16_t;
  auto stashAndLoad(FunctionIR &func, uint32_t reg, SpillMechanism smech)
      -> void;
  std::vector<bool> usedRegisters;
  std::uint16_t next;
  std::unordered_map<uint32_t, uint32_t> virtToReg;
  std::unordered_map<uint32_t, uint32_t> regToVirt;
  std::unordered_map<uint32_t, uint32_t> virtToStackOffset;
  uint32_t offset = 0;
};
} // namespace Fanta
