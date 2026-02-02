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

  std::array<std::uint32_t, 8> registers{0, 0, 0, 0, 0, 0, 0, 0};

  Memory ram; //16MB
private:
  std::uint16_t PC = 0;
};
