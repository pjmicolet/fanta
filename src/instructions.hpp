#pragma once
#include <cstdint>
#include <array>

namespace Instructions {
enum class Opcode {
  HALT,
  ADD,
  MOV,
  SUB,
  JMP
};

struct Decode {
  // Instruction format is as follows:
  // [opcode:6bit][dest:5bit][src1:5bit]
  //                                    [imm:16bit]
  //                                    [src2:5bit][unused:5bit]
  uint32_t raw;
  constexpr Decode(uint32_t d) : raw(d) {}

  constexpr auto getOpcode() const -> uint8_t {
    return (raw >> 26) & 0x3F;
  }

  constexpr auto getDestSrc() const -> uint8_t {
    return (raw >> 21) & 0x1F;
  }

  constexpr auto getS1() const -> uint8_t {
    return (raw >> 16) & 0x1F;
  }

  constexpr auto getS2() const -> uint8_t {
    return (raw >> 11) & 0x1F;
  }

  constexpr auto getImm() const -> uint16_t {
    return raw & (0xFF);
  }
};

template<std::uint8_t ID> struct Reg{};
template<std::uint16_t LIT> struct Literal{};

struct Halt {
  // So far assuming that HALT will be 4 bytes just because it
  // makes fetching the next inst easier.
  static constexpr auto emit() {
    return 0;
  }
};

template<typename Dest, typename Src1, typename Src2>
struct Add {};

template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>
struct Add<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x1 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t src2 = (Reg2 & 0x1F) << 11;
    return opcode | dest | src1 | src2;
  }
};

template<uint8_t Dest, uint8_t Reg1, uint16_t Imm>
struct Add<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x2 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFF;
    return opcode | dest | src1 | imm;
  }
};

template<typename Dest, typename Src1>
struct Mov {};

template<uint8_t Dest, uint8_t Reg1>
struct Mov<Reg<Dest>, Reg<Reg1> > {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x3 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    return opcode | dest | src1 ;
  }
};

template<uint8_t Dest, uint16_t Imm>
struct Mov<Reg<Dest>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x4 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t imm = Imm & 0xFF;
    return opcode | dest | imm;
  }
};

template<typename Dest, typename Src1, typename Src2>
struct Sub {};

template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>
struct Sub<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x5 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t src2 = (Reg2 & 0x1F) << 11;
    return opcode | dest | src1 | src2;
  }
};

template<uint8_t Dest, uint8_t Reg1, uint8_t Imm>
struct Sub<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x6 << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFF;
    return opcode | dest | src1 | imm;
  }
};

template<typename Dest>
struct Jmp {};

template<uint16_t Dest>
struct Jmp<Literal<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x7 << 26);
    constexpr uint32_t dest = (Dest & 0xFF);
    return opcode | dest;
  }
};


}
