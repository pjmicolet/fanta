#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>

struct Memory {
  Memory(std::size_t size) : memory(size) {}
  auto write32(std::size_t base_addr, uint32_t data) {
      std::memcpy(&memory[base_addr], &data, sizeof(uint32_t));
  }

  auto read32(std::size_t base_addr) -> uint32_t {
    uint32_t val;
    std::memcpy(&val, &memory[base_addr], sizeof(std::uint32_t)); 
    return val;
  }
  
private:
  std::vector<std::uint8_t> memory; //16MB
};

struct CPU {

  CPU() : ram(16*1024*1024) {}
  constexpr auto fetch() {
    auto data = ram.read32(PC);
    PC+=4;
    return data;
  }

  constexpr auto store(uint32_t addr, uint32_t val) -> void {
    ram.write32(addr, val);
  }

  constexpr auto load(uint32_t addr) -> std::uint32_t {
    return ram.read32(addr);
  }

  template<std::size_t S>
  constexpr auto load_rom(const std::array<uint32_t, S>& data) {
    for(std::size_t i = 0; i < S; i++) {
      ram.write32(i*4, data[i]);
    }
  }

  auto run_cycle() -> void;
  auto run_until_halt() -> void;

  auto get_prev_pc() -> std::uint32_t {
    return PC - 4; 
  }

  auto set_pc(uint32_t pc_val) -> void {
    PC = pc_val;
  }

  auto is_zero_set() -> bool {
    return status_reg[3] == 1;
  }

  auto is_neg_set() -> bool {
    return status_reg[1] == 1;
  }

  auto set_zero(uint32_t val) -> void {
    status_reg[3] = val == 0 ? 1 : 0;
  }

  auto set_neg(uint32_t val) -> void {
    status_reg[1] = static_cast<int32_t>(val) < 0 ? 1 : 0;
  }

  auto set_carry(uint32_t val) -> void {
    status_reg[3] = val; 
  }

  auto is_carry_set() -> bool {
    return status_reg[3]; 
  }

  std::array<std::uint32_t, 8> registers{0, 0, 0, 0, 0, 0, 0, 0};
  // Status regs are:
  // 0: Zero
  // 1: Negative
  // 2: Overflow (probably)
  // 4: Carry
  std::array<std::uint8_t, 4> status_reg{0,0,0,0};

  bool halted = false;

  Memory ram; //16MB
private:
  std::uint32_t PC = 0;
};
