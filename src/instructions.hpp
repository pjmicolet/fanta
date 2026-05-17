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
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::THREE_OP, reg_op, imm_op});

#define TWO_OP_INST( class_name, mnemonic, reg_op, imm_op) \
template<typename Dest, typename Src1>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::TWO_OP, reg_op, imm_op});

#define JUMP_INST( class_name, mnemonic, op) \
template<typename Dest>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::JUMP, op, 0});

#define BRANCH_INST( class_name, mnemonic, op) \
template<typename Dest>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::BRANCH, op, 0});

#define MEM_INST( class_name, mnemonic, op) \
template<typename SrcVal, typename DestMem, typename Offset>\
struct class_name {};\
static inline bool reg##class_name = Registry::register_inst(mnemonic, { InstFormat::MEM, op, 0});

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
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | dest | src1 | imm;
  }
};

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
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | dest | imm;
  }
};

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
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | dest | src1 | imm;
  }
};

template<std::int32_t Dest>
struct Jmp<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x7 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<std::int32_t Dest>
struct JmpRel<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x13 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<uint8_t SrcReg, uint8_t DestRegAddr, uint16_t Imm>
struct Store<Reg<SrcReg>, Reg<DestRegAddr>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x8 << 26);
    constexpr uint32_t srcval = (SrcReg & 0x1F) << 21;
    constexpr uint32_t destAddr = (DestRegAddr & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | srcval | destAddr | imm;
  }
};

template<uint8_t DestReg, uint8_t SrcRegAddr, uint16_t Imm>
struct Load<Reg<DestReg>, Reg<SrcRegAddr>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x9 << 26);
    constexpr uint32_t data_reg = (SrcRegAddr & 0x1F) << 21;
    constexpr uint32_t data_addr = (DestReg & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | data_reg | data_addr | imm;
  }
};

template<std::int32_t Dest>
struct Beq<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xA << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<std::int32_t Dest>
struct Bne<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xB << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<std::int32_t Dest>
struct Bec<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x10 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};


template<uint8_t Dest, uint8_t Reg1, uint8_t Reg2>
struct Lsh<Reg<Dest>, Reg<Reg1>, Reg<Reg2>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xC << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t src2 = (Reg2 & 0x1F) << 11;
    return opcode | dest | src1 | src2;
  }
};

template<uint8_t Dest, uint8_t Reg1, uint16_t Imm>
struct Lsh<Reg<Dest>, Reg<Reg1>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xD << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | dest | src1 | imm;
  }
};

template<uint8_t Dest, uint8_t Reg1>
struct Cmp<Reg<Dest>, Reg<Reg1> > {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xE << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t src1 = (Reg1 & 0x1F) << 16;
    return opcode | dest | src1 ;
  }
};

template<uint8_t Dest, uint16_t Imm>
struct Cmp<Reg<Dest>, Literal<Imm>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0xF << 26);
    constexpr uint32_t dest = (Dest & 0x1F) << 21;
    constexpr uint32_t imm = Imm & 0xFFFF;
    return opcode | dest | imm;
  }
};

template<std::int32_t Dest>
struct Bmi<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x11 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<std::int32_t Dest>
struct Bpl<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x12 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

template<std::int32_t Dest>
struct Call<Target<Dest>> {
  static constexpr auto emit() {
    constexpr uint32_t opcode = (0x15 << 26);
    constexpr uint32_t dest = (Dest & 0x3FFFFFF);
    return opcode | dest;
  }
};

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
