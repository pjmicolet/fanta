#pragma once
#include <cstdint>

enum class Opcode {
  HALT,
  MOV,
  ADD,
  SUB,
  JMP
};

template<std::uint8_t ID> struct Reg{};
template<std::uint8_t LIT> struct Literal{};
