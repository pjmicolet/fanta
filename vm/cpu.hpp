#pragma once
#include "bus.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

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

  auto from(std::size_t base_addr) -> uint8_t * { return &memory[base_addr]; }

  auto getSize() const -> std::size_t { return memory.size(); }

private:
  std::vector<std::uint8_t> memory; // 32MB
};

template <typename BType> struct BaseCPU {
  enum FLAG : uint8_t { ZERO, NEGATIVE, OVFL, CARRY };

  BaseCPU() : ram(32 * 1024 * 1024) {
    registers.fill(0);
    registers[16] = 0x7FFFFC; // word aligned
  }
  constexpr auto fetch() {
    auto data = ram.read32(PC);
    PC += 4;
    return data;
  }

  constexpr auto store(uint32_t addr, uint32_t val) -> void {
    if (addr >= bus.lowest) [[unlikely]] {
      bus.write(addr, val);
      return;
    }
    ram.write32(addr, val);
  }

  constexpr auto load(uint32_t addr) -> std::uint32_t {
    if (addr >= bus.lowest) [[unlikely]] {
      return bus.read(addr).value_or(0); // unmapped device space reads 0
    }
    return ram.read32(addr);
  }

  template <std::size_t S>
  constexpr auto load_rom(const std::array<uint32_t, S> &data) {
    for (std::size_t i = 0; i < S; i++) {
      ram.write32(i * 4, data[i]);
    }
  }

  auto run_cycle() -> void;
  auto run_until_halt() -> void;

  auto get_vram() { return ram.from(0x800000); }

  auto get_prev_pc() -> std::uint32_t { return PC - 4; }

  auto check_arith(uint32_t s1, uint32_t s2, uint32_t res, bool isSub) -> void {
    set_zero(res);
    set_neg(res);

    if (isSub) {
      set_carry(s1 >= s2);
      set_overflow(((s1 ^ s2) & (s1 ^ res)) & 0x80000000);
    } else {
      set_carry(res < s1);
      set_overflow(((s1 ^ res) & (s2 ^ res)) & 0x80000000);
    }
  }

  auto get_pc() const -> uint32_t { return PC; }

  auto get_sp() const -> uint32_t { return registers[16]; }

  auto set_pc(uint32_t pc_val) -> void { PC = pc_val; }

  auto flag_check(FLAG f, bool isNeg) {
    bool result = false;
    switch (f) {
    case ZERO:
      result = is_zero_set();
      break;
    case NEGATIVE:
      result = is_neg_set();
      break;
    case OVFL:
      result = is_overflow_set();
      break;
    case CARRY:
      result = is_carry_set();
      break;
    }
    return isNeg ? !result : result;
  }

  auto is_zero_set() -> bool { return status_reg[0] == 1; }

  auto is_neg_set() -> bool { return status_reg[1] == 1; }

  auto is_overflow_set() -> bool { return status_reg[2] == 1; }

  auto is_carry_set() -> bool { return status_reg[3] == 1; }

  auto set_zero(uint32_t val) -> void { status_reg[0] = val == 0 ? 1 : 0; }

  auto set_neg(uint32_t val) -> void {
    status_reg[1] = static_cast<int32_t>(val) < 0 ? 1 : 0;
  }

  auto set_overflow(uint32_t val) -> void { status_reg[2] = val ? 1 : 0; }

  auto set_carry(uint32_t val) -> void { status_reg[3] = val ? 1 : 0; }

  auto push_stack(uint32_t val) {
    ram.write32(registers[16], val);
    registers[16] -= 4;
  }

  auto push_stack() {
    ram.write32(registers[16], PC);
    registers[16] -= 4;
  }

  auto pop_stack() -> std::uint32_t {
    registers[16] += 4;
    return ram.read32(registers[16]);
  }

  std::array<std::uint32_t, 17> registers{0, 0, 0, 0, 0, 0, 0, 0,       0,
                                          0, 0, 0, 0, 0, 0, 0, 0x7FFFFF};
  // Status regs are:
  // 0: Zero
  // 1: Negative
  // 2: Overflow (probably)
  // 3: Carry
  std::array<std::uint8_t, 4> status_reg{0, 0, 0, 0};

  static constexpr size_t NUM_OF_CIP_INTERRUPTS = 1;

  // Interrupts are:
  // 0: VBLANK
  std::array<std::uint8_t, NUM_OF_CIP_INTERRUPTS> cip_interrupts{0};

  bool halted = false;

  Memory ram; // 32MB

  uint32_t inst_count = 0;

  BType bus;

private:
  std::uint32_t PC = 0;
};

// The default machine: a serial port at the start of the device area, right
// above the 32MB of RAM.
using BasicBus = Bus<Mapped<BasicSerial<>, 0x0200'0000>>;

// The rest of the codebase (execute.hpp, TUI, tests, tools) uses this name.
// run_cycle/run_until_halt are explicitly instantiated for it in cpu.cpp.
using CPU = BaseCPU<BasicBus>;
