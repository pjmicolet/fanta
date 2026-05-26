#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Instructions {

enum InstFormat { THREE_OP, TWO_OP, MEM, JUMP, BRANCH, HALT, RET };

struct InstMetadata {
  InstFormat fmt;
  uint8_t reg;
  uint8_t imm;
};

#define THREE_OP_INST( class_name, mnemonic, reg_op, imm_op) \
template<typename Dest, typename Src1, typename Src2>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::THREE_OP, reg_op, imm_op});\
\
template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>\
struct class_name<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {\
  static constexpr auto emit() {\
    return Emitter::three_op(reg_op, Dest, Reg1, Reg2);\
  }\
};\
\
template<uint8_t Dest, uint8_t Reg1, uint16_t Imm>\
struct class_name<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {\
  static constexpr auto emit() {\
    return Emitter::three_op_imm(imm_op, Dest, Reg1, Imm);\
  }\
};\

#define TWO_OP_INST( class_name, mnemonic, reg_op, imm_op) \
template<typename Dest, typename Src1>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::TWO_OP, reg_op, imm_op});\
\
template<uint8_t Dest, uint8_t Reg1>\
struct class_name<Reg<Dest>, Reg<Reg1>> {\
  static constexpr auto emit() {\
    return Emitter::two_op(reg_op, Dest, Reg1);\
  }\
};\
\
template<uint8_t Dest, uint16_t Imm>\
struct class_name<Reg<Dest>, Literal<Imm>> {\
  static constexpr auto emit() {\
    return Emitter::two_op_imm(imm_op, Dest, Imm);\
  }\
};\


#define JUMP_INST( class_name, mnemonic, op) \
template<typename Dest>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::JUMP, op, 0});\
\
template<int32_t Dest>\
struct class_name<Target<Dest>> { static constexpr auto emit() { return Emitter::branch(op, Dest);}\
};\

#define BRANCH_INST( class_name, mnemonic, op) \
template<typename Dest>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::BRANCH, op, 0});\
\
template<int32_t Dest>\
struct class_name<Target<Dest>> { static constexpr auto emit() { return Emitter::branch(op, Dest);}\
};\


#define MEM_INST( class_name, mnemonic, op) \
template<typename SrcVal, typename DestMem, typename Offset>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::MEM, op, 0});\
\
template<uint8_t Dest, uint8_t Reg1, uint16_t Imm>\
struct class_name<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {\
  static constexpr auto emit() {\
    return Emitter::three_op_imm(op, Dest, Reg1, Imm);\
  }\
};\



#define HALT_INST( class_name, mnemonic, op) \
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::HALT, op, 0});

#define RET_INST( class_name, mnemonic, op) \
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::RET, op, 0});

struct Registry {
  static inline auto& get() {
    static std::unordered_map<std::string, InstMetadata> map;
    return map;
  }

  static auto& fetch(const std::string& key) {
    return get().at(key);
  }

  static inline auto register_inst(std::string mnem, InstMetadata fmt) {
    get()[mnem] = fmt;

    return true;

  }
};

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
  LSH,
  CMP,
  BEC,
  BMI,
  BPL,
  JREL,
  NOP,
  CALL,
  RET,
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
    return raw & (0xFFFF);
  }
};

// constexpr assembler
template<std::uint8_t ID> struct Reg{};
template<std::int16_t LIT = 0> struct Literal{};
template<std::int32_t TARGET = 0> struct Target{};

struct Halt {
  static constexpr auto emit() {
    return 0;
  }
};

struct Nop {
  static constexpr auto emit() {
    return 0x14 << 26;
  }
};

struct Emitter {
  static constexpr uint32_t three_op(uint8_t op, uint8_t d, uint8_t s1, uint8_t s2) {
    return uint32_t(op << 26) | uint32_t(d & 0x1F) << 21 | uint32_t(s1 & 0x1F) << 16 | uint32_t(s2 & 0x1F) << 11;
  }

  static constexpr uint32_t three_op_imm(uint8_t op, uint8_t d, uint8_t s1, uint16_t imm) {
    return uint32_t(op << 26) | uint32_t(d & 0x1f) << 21 | uint32_t(s1 & 0x1f) << 16 | uint32_t(imm & 0xffff);
  }

  static constexpr uint32_t branch(uint8_t op, int32_t dest) {
    return uint32_t(op << 26) | uint32_t(dest & 0x3FFFFFF);
  }

  static constexpr uint32_t two_op(uint8_t op, uint8_t d, uint8_t s1) {
    return uint32_t(op << 26) | uint32_t(d & 0x1F) << 21 | uint32_t(s1 & 0x1F) << 16;
  }

  static constexpr uint32_t two_op_imm(uint8_t op, uint8_t d, uint16_t s1) {
    return uint32_t(op << 26) | uint32_t(d & 0x1F) << 21 | uint32_t(s1 & 0xFFFF);
  }
};

static inline bool regHalt = Registry::register_inst("HALT", { InstFormat::HALT, 0, 0});
static inline bool nop = Registry::register_inst("NOP", { InstFormat::HALT, 0x14, 0});
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
THREE_OP_INST(Lsh, "LSH", 0xC, 0xD)
TWO_OP_INST(Cmp, "CMP", 0xE, 0xF)
BRANCH_INST(Bmi, "BMI", 0x11)
BRANCH_INST(Bpl, "BPL", 0x12)
BRANCH_INST(Call, "CALL", 0x15)
static inline bool ret = Registry::register_inst("RET", { InstFormat::RET, 0x16, 0});
THREE_OP_INST(And, "AND", 0x17, 0x18)
THREE_OP_INST(Or, "OR", 0x19, 0x1A)
THREE_OP_INST(Xor, "XOR", 0x1B, 0x1C)

struct Ret {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x16 << 26);
    return opcode;
  }
};

// runtime assembler
inline auto parse_three(uint32_t op, uint32_t dest, uint32_t s1, uint32_t s2_or_imm, bool using_imm) -> std::uint32_t {
  uint32_t res = (op << 26) | ((dest & 0x1F) << 21) | ((s1 & 0x1F) << 16);
  if (using_imm) res |= (s2_or_imm & 0xFFFF);
  else        res |= ((s2_or_imm & 0x1F) << 11);
  return res;
}

inline auto parse_two(uint32_t op, uint32_t dest, uint32_t other, bool isImm) -> std::uint32_t {
  uint32_t opcode = (op << 26);
  uint32_t destr = (dest & 0x1F) << 21;
  uint32_t second = isImm ? other & 0xFFFF : (other & 0x1F) << 16;
  return opcode | destr | second;
}

inline auto parse_one(uint32_t op, uint32_t dest) -> std::uint32_t {
  return (op << 26) | (dest & 0x3FFFFFF);
}
}
