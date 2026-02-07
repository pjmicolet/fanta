#pragma once
#include <cstdint>
#include <array>

namespace Instructions {
enum class Opcode {
  HALT,
  ADD,
  MOV,
  SUB,
  JMP,
  STORE,
  LOAD,
  BEQ,
  BNE,
};

struct Decode {
  // Instruction format is as follows:
  // [opcode:6bit][dest:5bit][src1:5bit]
  //                                    [imm:16bit]
  //                                    [src2:5bit][unused:5bit]
  // For store we have
  // [opcode:6bit][reg_with_val:5bit][dest_base:5bit][offset:16bit]
  // For load it's
  // [opcode:6bit][reg_where_value_is_stored:5bit][src_base:5bit][offset:16bit]
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
template<std::uint16_t LIT = 0> struct Literal{};

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

template<uint8_t Dest, uint8_t Reg1, uint16_t Imm>
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
    constexpr uint32_t dest = (Dest & 0x3FFF);
    return opcode | dest;
  }
};

template<typename SrcVal, typename DestMem, typename Offset>
struct Store{};

template<uint8_t SrcReg, uint8_t DestRegAddr, uint16_t Imm>
struct Store<Reg<SrcReg>, Reg<DestRegAddr>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x8 << 26);
    constexpr uint32_t srcval = (SrcReg & 0x1F) << 21;
    constexpr uint32_t destAddr = (DestRegAddr & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFF;
    return opcode | srcval | destAddr | imm;
  }
};

template<typename DestReg, typename SourceMem, typename Offset>
struct Load{};

template<uint8_t DestReg, uint8_t SrcRegAddr, uint16_t Imm>
struct Load<Reg<DestReg>, Reg<SrcRegAddr>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x9 << 26);
    constexpr uint32_t srcval = (DestReg & 0x1F) << 21;
    constexpr uint32_t destAddr = (SrcRegAddr & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFF;
    return opcode | srcval | destAddr | imm;
  }
};

template<typename Dest>
struct Beq {};

template<uint16_t Dest>
struct Beq<Literal<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xA << 26);
    constexpr uint32_t dest = (Dest & 0x3FFF);
    return opcode | dest;
  }
};

template<typename Dest>
struct Bne {};

template<uint16_t Dest>
struct Bne<Literal<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xB << 26);
    constexpr uint32_t dest = (Dest & 0x3FFF);
    return opcode | dest;
  }
};

}
