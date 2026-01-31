#pragma once

#include "cpu.hpp"
#include "instructions.hpp"

struct RegisterMode {
  static auto fetch(CPU& cpu) {
    auto reg = cpu.fetch();
    return cpu.registers[reg];
  }

  static auto store(CPU& cpu, uint8_t val) -> void {
    auto reg = cpu.fetch();
    cpu.registers[reg] = val;
  }
};

struct ImmediateMode {
  static auto fetch(CPU& cpu) {
    return cpu.fetch();
  }
};

template<typename DestOp, typename Src1Op, typename Src2Op>
struct OpAdd {
  static auto exec(CPU& cpu) {
    auto val1 = Src1Op::fetch(cpu);
    auto val2 = Src2Op::fetch(cpu);
    auto result = val1+val2;
    DestOp::store(cpu, result);
  }
};

using AddReg = OpAdd<RegisterMode, RegisterMode, RegisterMode>;
using AddImm = OpAdd<RegisterMode, RegisterMode, ImmediateMode>;

template<typename DestOp, typename Src1Op, typename Src2Op>
struct OpSub {
  static auto exec(CPU& cpu) {
    auto val1 = Src1Op::fetch(cpu);
    auto val2 = Src2Op::fetch(cpu);
    auto result = val1-val2;
    DestOp::store(cpu, result);
  }
};

using SubReg = OpSub<RegisterMode, RegisterMode, RegisterMode>;
using SubImm = OpSub<RegisterMode, RegisterMode, ImmediateMode>;

template<typename DestOp, typename Src1Op>
struct OpMov {
  static auto exec(CPU& cpu) {
    auto val1 = Src1Op::fetch(cpu);
    auto result = val1;
    DestOp::store(cpu, result);
    cpu.fetch(); // instruction needs an extra byte
  }
};

using MovReg = OpMov<RegisterMode, RegisterMode>;
using MovImm = OpMov<RegisterMode, ImmediateMode>;
