#pragma once

#include <vector>

namespace Fanta {

  struct Allocator {
    Allocator() : usedRegisters(15, false), next(0) {}

    auto getNextAvailable() -> std::uint16_t {
      if(usedReisters[next]) {
      }

      auto allocatedReg = next;
      next = (next + 1) % 15;
      return allocatedReg;
      
    }
    std::vector<bool> usedRegisters;
    std::uint16_t next;
  };
}
