#pragma once
#include <cstdint>
#include <array>

struct CPU {

  constexpr auto fetch() {
    return ram[PC++];
  }

  template<std::size_t S>
  constexpr auto load_rom(const std::array<uint32_t, S>& data) {
    if constexpr( S > 2000) {
      static_assert(false, "You've tried to dump a rom larger than 2000 bytes");
    }
    for(std::size_t i = 0; i < S; i++) {
      ram[i] = data[i];
    }
  }

  auto run_cycle() -> void;

  std::array<std::uint32_t, 8> registers{0, 0, 0, 0, 0, 0, 0, 0};

private:
  std::uint16_t PC;
  std::array<std::uint32_t, 2000> ram;
};
