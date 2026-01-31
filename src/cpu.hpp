#pragma once
#include <cstdint>
#include <array>

struct CPU {
  auto fetch() {
    return ram[PC++];
  }

  std::array<std::uint8_t, 8> registers;
private:
  std::uint16_t PC;
  std::array<std::uint8_t, 2000> ram;
};
