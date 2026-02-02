#pragma once

#include "cpu.hpp"
#include "instructions.hpp"

struct DecodeDest {
  static auto store(CPU& cpu, std::uint32_t inst, uint32_t result) -> void {
    auto reg = (inst >> 21) & 0x1F;
    cpu.registers[reg] = result;
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
    return inst & 0xFF;
  }
};

struct DecodeStorageDest {
  static auto store(CPU& cpu, std::uint32_t inst, uint32_t result) -> void {
    auto base = cpu.registers[(inst >> 16) & 0x1F] + inst & 0xFF;
    cpu.store(base, result);
  }
};

struct DecodeLoadSource {
  static auto decode(CPU& cpu, std::uint32_t inst) -> std::uint32_t {
    auto base = cpu.registers[(inst >> 16) & 0x1F] + inst & 0xFF;
    return cpu.load(base);
  }
};

template<typename DestDecoder, typename S1Decoder, typename OptDecoder>
struct OpAdd {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto s1_data = S1Decoder::decode(cpu, inst);
    auto opt_data = OptDecoder::decode(cpu, inst);
    auto result = s1_data + opt_data;
    DestDecoder::store(cpu, inst, result);
  }
};

using AddReg = OpAdd<DecodeDest, DecodeSource1, DecodeSource2>;
using AddImm = OpAdd<DecodeDest, DecodeSource1, DecodeImm>;

template<typename DestDecoder, typename S1Decoder, typename OptDecoder>
struct OpSub {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto s1_data = S1Decoder::decode(cpu, inst);
    auto opt_data = OptDecoder::decode(cpu, inst);
    auto result = s1_data - opt_data;
    DestDecoder::store(cpu, inst, result);
  }
};

using SubReg = OpSub<DecodeDest, DecodeSource1, DecodeSource2>;
using SubImm = OpSub<DecodeDest, DecodeSource1, DecodeImm>;

template<typename DestDecoder, typename OptDecoder>
struct OpMov {
  static auto exec(CPU& cpu, uint32_t inst) {
    DestDecoder::store(cpu, inst, OptDecoder::decode(cpu,inst));
  }
};

using MovReg = OpMov<DecodeDest, DecodeSource1>;
using MovImm = OpMov<DecodeDest, DecodeImm>;

template<typename SrcVal, typename DestAddr>
struct OpMem {
  static auto exec(CPU& cpu, uint32_t inst) {
    auto val = SrcVal::decode(cpu, inst);
    DestAddr::store(cpu, inst, val);
  }
};

// This is some magic here
using StoreReg = OpMem<DecodeSource1, DecodeStorageDest>;
using LoadReg = OpMem<DecodeLoadSource, DecodeDest>;

// One of the few non composable
struct Jmp {
  static auto exec(CPU& cpu, uint32_t inst) {
    cpu.set_pc(inst & 0x1FFF);
  }
};
