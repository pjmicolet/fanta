#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Instructions {

enum InstFormat : std::uint8_t {
  THREE_OP,
  TWO_OP,
  MEM,
  JUMP,
  BRANCH,
  HALT,
  RET,
  STACK_INST,
  NOP,
};

struct InstMetadata {
  InstFormat fmt;
  uint8_t reg;
  uint8_t imm;
};

struct InstEntry {
  std::string_view mnemonic;
  InstMetadata meta;
};

#define THREE_OP_INST(class_name, mnemonic, reg_op, imm_op)                    \
  template <typename Dest, typename Src1, typename Src2> struct class_name {}; \
                                                                               \
  template <uint8_t Dest, uint8_t Reg1, uint8_t Reg2>                          \
  struct class_name<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {                         \
    static constexpr auto emit() {                                             \
      return Emitter::three_op(reg_op, Dest, Reg1, Reg2);                      \
    }                                                                          \
  };                                                                           \
                                                                               \
  template <uint8_t Dest, uint8_t Reg1, int16_t Imm>                           \
  struct class_name<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {                      \
    static constexpr auto emit() {                                             \
      return Emitter::three_op_imm(imm_op, Dest, Reg1, Imm);                   \
    }                                                                          \
  };

#define TWO_OP_INST(class_name, mnemonic, reg_op, imm_op)                      \
  template <typename Dest, typename Src1> struct class_name {};                \
                                                                               \
  template <uint8_t Dest, uint8_t Reg1>                                        \
  struct class_name<Reg<Dest>, Reg<Reg1>> {                                    \
    static constexpr auto emit() {                                             \
      return Emitter::two_op(reg_op, Dest, Reg1);                              \
    }                                                                          \
  };                                                                           \
                                                                               \
  template <uint8_t Dest, int16_t Imm>                                         \
  struct class_name<Reg<Dest>, Literal<Imm>> {                                 \
    static constexpr auto emit() {                                             \
      return Emitter::two_op_imm(imm_op, Dest, Imm);                           \
    }                                                                          \
  };

#define JUMP_INST(class_name, mnemonic, op)                                    \
  template <typename Dest> struct class_name {};                               \
                                                                               \
  template <int32_t Dest> struct class_name<Target<Dest>> {                    \
    static constexpr auto emit() { return Emitter::single_inst(op, Dest); }    \
  };

#define BRANCH_INST(class_name, mnemonic, op)                                  \
  template <typename Dest> struct class_name {};                               \
                                                                               \
  template <int32_t Dest> struct class_name<Target<Dest>> {                    \
    static constexpr auto emit() { return Emitter::single_inst(op, Dest); }    \
  };

#define STACK_INST(class_name, mnemonic, op)                                   \
  template <typename Dest> struct class_name {};                               \
                                                                               \
  template <int32_t Dest> struct class_name<Target<Dest>> {                    \
    static constexpr auto emit() { return Emitter::single_inst(op, Dest); }    \
  };

#define MEM_INST(class_name, mnemonic, op)                                     \
  template <typename SrcVal, typename DestMem, typename Offset>                \
  struct class_name {};                                                        \
                                                                               \
  template <uint8_t Dest, uint8_t Reg1, int16_t Imm>                           \
  struct class_name<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {                      \
    static constexpr auto emit() {                                             \
      return Emitter::three_op_imm(op, Dest, Reg1, Imm);                       \
    }                                                                          \
  };

#define HALT_INST(class_name, mnemonic, op)
#define NOP_INST(class_name, mnemonic, op)

#define RET_INST(class_name, mnemonic, op)

/// Sign-extends the 26-bit offset field (bits 0-25) of a branch, JREL or CALL
/// word. Uses only unsigned arithmetic, so there's no signed shift to reason
/// about.
constexpr auto sign_extend26(uint32_t word) -> int32_t {
  constexpr uint32_t kSignBit = 1U << 25;
  return static_cast<int32_t>(((word & 0x3FFFFFFU) ^ kSignBit) - kSignBit);
}
static_assert(sign_extend26(0x3FFFFF8) == -8);
static_assert(sign_extend26(0x0000008) == 8);
static_assert(sign_extend26(0xFC000000 | 0x1FFFFFF) == 0x1FFFFFF);
static_assert(sign_extend26(0x2000000) == -0x2000000);

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

  [[nodiscard]] constexpr auto getOpcode() const -> uint8_t {
    return (raw >> 26) & 0x3F;
  }

  [[nodiscard]] constexpr auto getDestSrc() const -> uint8_t {
    return (raw >> 21) & 0x1F;
  }

  [[nodiscard]] constexpr auto getS1() const -> uint8_t {
    return (raw >> 16) & 0x1F;
  }

  [[nodiscard]] constexpr auto getS2() const -> uint8_t {
    return (raw >> 11) & 0x1F;
  }

  [[nodiscard]] constexpr auto getImm() const -> uint16_t {
    return raw & 0xFFFF;
  }
};

// constexpr assembler
template <std::uint8_t ID> struct Reg {};
template <std::int16_t LIT = 0> struct Literal {};
template <std::int32_t TARGET = 0> struct Target {};

struct Halt {
  static constexpr auto emit() { return 0; }
};

struct Nop {
  static constexpr auto emit() { return 0x14 << 26; }
};

struct Emitter {
  // Unsigned masks (0x1FU) keep every operand unsigned, so no casts or
  // signed shifts are needed; only the opcode is widened before shifting.
  static constexpr uint32_t three_op(uint8_t op, uint8_t d, uint8_t s1,
                                     uint8_t s2) {
    return (static_cast<uint32_t>(op) << 26) | ((d & 0x1FU) << 21) |
           ((s1 & 0x1FU) << 16) | ((s2 & 0x1FU) << 11);
  }

  static constexpr uint32_t three_op_imm(uint8_t op, uint8_t d, uint8_t s1,
                                         uint16_t imm) {
    return (static_cast<uint32_t>(op) << 26) | ((d & 0x1FU) << 21) |
           ((s1 & 0x1FU) << 16) | (imm & 0xFFFFU);
  }

  static constexpr uint32_t single_inst(uint8_t op, uint32_t dest) {
    return (static_cast<uint32_t>(op) << 26) | (dest & 0x3FFFFFFU);
  }

  static constexpr uint32_t two_op(uint8_t op, uint8_t d, uint8_t s1) {
    return (static_cast<uint32_t>(op) << 26) | ((d & 0x1FU) << 21) |
           ((s1 & 0x1FU) << 16);
  }

  static constexpr uint32_t two_op_imm(uint8_t op, uint8_t d, uint16_t s1) {
    return (static_cast<uint32_t>(op) << 26) | ((d & 0x1FU) << 21) |
           (s1 & 0xFFFFU);
  }
};

THREE_OP_INST(Add, "ADD", 0x1, 0x2)
TWO_OP_INST(Mov, "MOV", 0x3, 0x4)
THREE_OP_INST(Sub, "SUB", 0x5, 0x6)
JUMP_INST(Jmp, "JMP", 0x7)
JUMP_INST(JmpRel, "JREL", 0x13)
MEM_INST(Store, "STORE", 0x8)
MEM_INST(Load, "LOAD", 0x9)
BRANCH_INST(Beq, "BEQ", 0xA)
BRANCH_INST(Bne, "BNE", 0xB)
BRANCH_INST(Bec, "BEC", 0x10)
BRANCH_INST(Bnc, "BNC", 0x20)
THREE_OP_INST(Lsh, "LSH", 0xC, 0xD)
TWO_OP_INST(Cmp, "CMP", 0xE, 0xF)
BRANCH_INST(Bmi, "BMI", 0x11)
BRANCH_INST(Bpl, "BPL", 0x12)
BRANCH_INST(Call, "CALL", 0x15)
THREE_OP_INST(And, "AND", 0x17, 0x18)
THREE_OP_INST(Or, "OR", 0x19, 0x1A)
THREE_OP_INST(Xor, "XOR", 0x1B, 0x1C)
STACK_INST(Push, "PUSH", 0x1D)
STACK_INST(Pop, "POP", 0x1E)
JUMP_INST(Cip, "CIP", 0x1F)

struct Ret {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x16 << 26);
    return opcode;
  }
};

// Single source of truth for runtime instruction lookup. Every entry here
// mirrors one macro invocation (or manual struct) above -- kept in sync by
// hand since the macros above only need to emit compile-time assembler
// types, not runtime metadata. No dynamic initialization, no heap
// allocation, no unspecified init order: this table exists fully formed
// before main() runs.
inline constexpr std::array<InstEntry, 25> kInstructionTable{
    {
        {"HALT", {InstFormat::HALT, 0, 0}},
        {"NOP", {InstFormat::NOP, 0x14, 0}},
        {"ADD", {InstFormat::THREE_OP, 0x1, 0x2}},
        {"MOV", {InstFormat::TWO_OP, 0x3, 0x4}},
        {"SUB", {InstFormat::THREE_OP, 0x5, 0x6}},
        {"JMP", {InstFormat::JUMP, 0x7, 0}},
        {"JREL", {InstFormat::BRANCH, 0x13, 0}},
        {"STORE", {InstFormat::MEM, 0x8, 0}},
        {"LOAD", {InstFormat::MEM, 0x9, 0}},
        {"BEQ", {InstFormat::BRANCH, 0xA, 0}},
        {"BNE", {InstFormat::BRANCH, 0xB, 0}},
        {"BEC", {InstFormat::BRANCH, 0x10, 0}},
        {"BNC", {InstFormat::BRANCH, 0x20, 0}},
        {"LSH", {InstFormat::THREE_OP, 0xC, 0xD}},
        {"CMP", {InstFormat::TWO_OP, 0xE, 0xF}},
        {"BMI", {InstFormat::BRANCH, 0x11, 0}},
        {"BPL", {InstFormat::BRANCH, 0x12, 0}},
        {"CALL", {InstFormat::BRANCH, 0x15, 0}},
        {"RET", {InstFormat::RET, 0x16, 0}},
        {"AND", {InstFormat::THREE_OP, 0x17, 0x18}},
        {"OR", {InstFormat::THREE_OP, 0x19, 0x1A}},
        {"XOR", {InstFormat::THREE_OP, 0x1B, 0x1C}},
        {"PUSH", {InstFormat::STACK_INST, 0x1D, 0}},
        {"POP", {InstFormat::STACK_INST, 0x1E, 0}},
        {"CIP", {InstFormat::JUMP, 0x1F, 0}},
    },
};

inline constexpr auto kOpcodeToMeta = [] {
  std::array<const InstMetadata *, kInstructionTable.size() * 2> t{};
  for (const auto &e : kInstructionTable) {
    if (t[e.meta.reg] != nullptr) [[unlikely]]
      throw std::runtime_error("Someone copied the same opcode twice");
    t[e.meta.reg] = &e.meta;
    if (e.meta.imm) {
      if (t[e.meta.imm] != nullptr) [[unlikely]]
        throw std::runtime_error("Someone copied the same imm opcode twice");
      t[e.meta.imm] = &e.meta;
    }
  }
  return t;
}();

struct Registry {
  static constexpr auto &get() { return kInstructionTable; }

  static constexpr auto fetch(std::string_view key) -> const InstMetadata & {
    for (const auto &entry : kInstructionTable) {
      if (entry.mnemonic == key)
        return entry.meta;
    }

    throw std::out_of_range("Unknown instruction mnemonic: " +
                            std::string(key));
  }

  static constexpr auto fetch(uint32_t opcode) -> const InstMetadata & {
    if (opcode >= kOpcodeToMeta.size() || kOpcodeToMeta[opcode] == nullptr)
        [[unlikely]]
      throw std::out_of_range("Unknown instruction opcode:" +
                              std::to_string(opcode));
    return *kOpcodeToMeta[opcode];
  }
};

// runtime assembler
inline auto parse_three(uint32_t op, uint32_t dest, uint32_t s1,
                        uint32_t s2_or_imm, bool using_imm) -> std::uint32_t {
  uint32_t res = (op << 26) | ((dest & 0x1F) << 21) | ((s1 & 0x1F) << 16);
  if (using_imm)
    res |= (s2_or_imm & 0xFFFF);
  else
    res |= ((s2_or_imm & 0x1F) << 11);
  return res;
}

inline auto parse_two(uint32_t op, uint32_t dest, uint32_t other, bool isImm)
    -> std::uint32_t {
  uint32_t opcode = (op << 26);
  uint32_t destr = (dest & 0x1F) << 21;
  uint32_t second = isImm ? other & 0xFFFF : (other & 0x1F) << 16;
  return opcode | destr | second;
}

inline auto parse_one(uint32_t op, uint32_t dest) -> std::uint32_t {
  return (op << 26) | (dest & 0x3FFFFFF);
}
} // namespace Instructions
