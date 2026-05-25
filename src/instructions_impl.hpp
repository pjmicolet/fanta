#pragma once

#include "cpu.hpp"

struct DecodeDest {
  static auto store(CPU& cpu, std::uint32_t inst, uint32_t result) -> void {
    auto reg = (inst >> 21) & 0x1F;
    cpu.registers[reg] = result;
  }
};

struct DecodeLoadDest {
  static auto store(CPU& cpu, std::uint32_t inst, uint32_t result) -> void {
    auto reg = (inst >> 21) & 0x1F;
    cpu.registers[reg] = result;
  }
};

struct DecodeS1Cmp {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    auto reg = (inst >> 21) & 0x1F;
    return cpu.registers[reg];
  }
};

struct DecodeSource1 {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    return cpu.registers[(inst >> 16) & 0x1F];
  }
};

struct DecodeSource2 {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    return cpu.registers[(inst >> 11) & 0x1F];
  }
};

struct DecodeImm {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    return inst & 0xFFFF;
  }
};

struct DecodeStorageDest {
  static auto store(CPU& cpu, std::uint32_t inst, uint32_t result) -> void {
    auto base = cpu.registers[(inst >> 16) & 0x1F] + (inst & 0xFFFF);
    cpu.store(base, result);
  }
};

struct DecodeLoadSource {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    auto base = cpu.registers[(inst >> 16) & 0x1F] + (inst & 0xFFFF);
    return cpu.load(base);
  }
};

enum OpType {
  ARITH_ADD,
  ARITH_SUB,
  LOGICAL,
  LSHIFT
};

template<typename DestDecoder, typename S1Decoder, typename OptDecoder, auto OpFunc, OpType type>
struct OpArithLogical {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto s1_data = S1Decoder::decode(cpu, inst);
    auto opt_data = OptDecoder::decode(cpu, inst);
    auto result = OpFunc(s1_data, opt_data);
    if constexpr(type == ARITH_ADD) {
      cpu.check_arith(s1_data, opt_data, result, false);
    } else if constexpr(type == ARITH_SUB){
      cpu.check_arith(s1_data, opt_data, result, true);
    } else if constexpr(type == LOGICAL) {
      cpu.set_neg(result);
      cpu.set_zero(result);
    } else if constexpr(type == LSHIFT){
      cpu.set_neg(result);
      cpu.set_carry(opt_data > 0 ? s1_data >> (32-opt_data)&0x1 : 0);
      cpu.set_zero(result);
    }
    DestDecoder::store(cpu, inst, result);
  }
};

template<typename DestDecoder, typename OptDecoder>
struct OpMov {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto data = OptDecoder::decode(cpu,inst);
    DestDecoder::store(cpu, inst, data);
    cpu.set_neg(data);
    cpu.set_zero(data);
  }
};

template<typename SrcVal, typename DestAddr>
struct OpMem {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto val = SrcVal::decode(cpu, inst);
    DestAddr::store(cpu, inst, val);
    cpu.set_neg(val);
    cpu.set_zero(val);
  }
};

// One of the few non composable
struct Jmp {
  static auto exec(CPU& cpu, uint32_t inst) {
    cpu.set_pc(inst & 0x3FFFFFF);
  }
};

auto parse_as_signed(uint32_t data) -> std::int32_t {
  std::int32_t temp = data;
  temp = temp << 6;
  return temp >> 6;
}

template<CPU::FLAG f, bool isNeg>
struct Branch {
  static auto exec(CPU& cpu, uint32_t inst) {
    if(cpu.flag_check(f,isNeg)) {
      auto prev = cpu.get_prev_pc();
      auto res = static_cast<uint32_t>(static_cast<int32_t>(prev) + parse_as_signed(inst & 0x3FFFFFF));
      cpu.set_pc(res);
    }
  }
};

struct Jrel {
  static auto exec(CPU& cpu, uint32_t inst) {
      auto prev = cpu.get_prev_pc();
      auto res = static_cast<uint32_t>(static_cast<int32_t>(prev) + parse_as_signed(inst & 0x3FFFFFF));
      cpu.set_pc(res);
  }
};

struct Halt {
  static auto exec(CPU& cpu, uint32_t inst) {
    cpu.halted = true;
  }
};

struct Nop {
  static auto exec(CPU& cpu, uint32_t inst) {
  }
};

template<typename DestDecoder, typename OptDecoder>
struct OpCmp {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto data = static_cast<std::int32_t>(DestDecoder::decode(cpu,inst));
    auto data2 = static_cast<std::int32_t>(OptDecoder::decode(cpu,inst));
    if(data == data2) {
      cpu.set_zero(0);
    } else {
      cpu.set_zero(1);
    }
    if(data > data2) {
      cpu.set_carry(1);
    } else {
      cpu.set_carry(0);
    }

    cpu.set_neg(data - data2);
  }
};

struct Ret {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto pc = cpu.pop_stack();
    cpu.set_pc(pc);
  }
};

struct Call {
  static auto exec(CPU& cpu, uint32_t inst) {
    cpu.push_stack();
    auto prev = cpu.get_prev_pc();
    auto res = static_cast<uint32_t>(static_cast<int32_t>(prev) + parse_as_signed(inst & 0x3FFFFFF));
    cpu.set_pc(res);
  }
};

constexpr auto op_plus = [](uint32_t a, uint32_t b) { return a + b; };
constexpr auto op_minus = [](uint32_t a, uint32_t b) { return a - b; };
constexpr auto op_and = [](uint32_t a, uint32_t b) { return a & b; };
constexpr auto op_lsh = [](uint32_t a, uint32_t b) { return a << b; };

using AddReg = OpArithLogical<DecodeDest, DecodeSource1, DecodeSource2, op_plus, ARITH_ADD>;
using AddImm = OpArithLogical<DecodeDest, DecodeSource1, DecodeImm, op_plus, ARITH_ADD>;

using SubReg = OpArithLogical<DecodeDest, DecodeSource1, DecodeSource2, op_minus, ARITH_SUB>;
using SubImm = OpArithLogical<DecodeDest, DecodeSource1, DecodeImm, op_minus, ARITH_SUB>;

using AndReg = OpArithLogical<DecodeDest, DecodeSource1, DecodeSource2, op_and, LOGICAL>;
using AndImm = OpArithLogical<DecodeDest, DecodeSource1, DecodeImm, op_and, LOGICAL>;

using LshReg = OpArithLogical<DecodeDest, DecodeSource1, DecodeSource2, op_lsh, LSHIFT>;
using LshImm = OpArithLogical<DecodeDest, DecodeSource1, DecodeImm, op_lsh, LSHIFT>;

using CmpReg = OpCmp<DecodeS1Cmp, DecodeSource1>;
using CmpImm = OpCmp<DecodeS1Cmp, DecodeImm>;

// This is some magic here
using StoreReg = OpMem<DecodeS1Cmp, DecodeStorageDest>;
using LoadReg = OpMem<DecodeLoadSource, DecodeLoadDest>;
using MovReg = OpMov<DecodeDest, DecodeSource1>;
using MovImm = OpMov<DecodeDest, DecodeImm>;

//Branches
using Beq = Branch<CPU::FLAG::ZERO, false>;
using Bne = Branch<CPU::FLAG::ZERO, true>;
using Bec = Branch<CPU::FLAG::CARRY, false>;
using Bmi = Branch<CPU::FLAG::NEGATIVE, false>;
using Bpl = Branch<CPU::FLAG::NEGATIVE, true>;
