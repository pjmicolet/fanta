#pragma once
#include <variant>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Fanta {
using InstOp = std::uint16_t; //Opcode
using Operand = std::uint16_t;

enum Source2Type {
  Register,
  Immediate
};

struct IROp {
  InstOp opcode;
  Operand destination;
  Operand source1;
  Operand source2;
  Source2Type s2type;
};

struct FuncParam {
  std::string_view name;
  Operand reg;
  std::uint32_t frameOffset;
};

struct FuncPrelude {
  std::vector<FuncParam> params;
  std::uint32_t localVarCount;
  std::vector<Operand> calleeRegs;
};

struct CallFunc {
  std::vector<FuncParam> args;
  std::string_view name;
  Operand dest;
};

struct FuncPostlude {
  std::vector<Operand> calleeRegs;
};

using IRInst = std::variant<IROp, FuncPrelude, CallFunc, FuncPostlude>;

struct IR {
  std::vector<IRInst> insts;
};

struct RegAllocIR {
  std::vector<IRInst> insts;
};
}
