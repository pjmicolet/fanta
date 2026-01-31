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

template<std::uint8_t ID> struct Reg{};
template<std::uint8_t LIT> struct Literal{};

struct Halt {
  // So far assuming that HALT will be 4 bytes just because it
  // makes fetching the next inst easier.
  static constexpr auto emit() {
    return std::array<uint8_t,4>{0x0, 0x0, 0x0, 0x0};
  }
};

template<typename Dest, typename Src1, typename Src2>
struct Add {};

template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>
struct Add<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x1, Reg1, Reg2, Dest};
  }
};

template<uint8_t Dest, uint8_t Reg1, uint8_t Imm>
struct Add<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x2, Reg1, Imm, Dest};
  }
};

template<typename Dest, typename Src1>
struct Mov {};

template<uint8_t Dest, uint8_t Reg1>
struct Mov<Reg<Dest>, Reg<Reg1> > {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x3, Reg1, Dest, 0x0};
  }
};

template<uint8_t Dest, uint8_t Imm>
struct Mov<Reg<Dest>, Literal<Imm>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x4, Imm, Dest, 0x0};
  }
};

template<typename Dest, typename Src1, typename Src2>
struct Sub {};

template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>
struct Sub<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x5, Reg1, Reg2, Dest};
  }
};

template<uint8_t Dest, uint8_t Reg1, uint8_t Imm>
struct Sub<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x6, Reg1, Imm, Dest};
  }
};

template<typename Dest, typename Dest2>
struct Jmp {};

template<uint8_t Dest, uint8_t Dest2>
struct Jmp<Literal<Dest>, Literal<Dest2>> {
  static constexpr auto emit() {
    return std::array<uint8_t, 4>{0x7, Dest, Dest2, 0x0};
  }
};


}
