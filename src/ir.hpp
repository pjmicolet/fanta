#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Fanta {
using InstOp = std::uint16_t; // Opcode
using Operand = std::uint16_t;

enum Source2Type { Register, Immediate };

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

struct CallFunc {
  std::vector<FuncParam> args;
  std::string_view name;
  Operand dest;
};

using IRInst = std::variant<IROp, CallFunc>;

using IRListing = std::vector<IRInst>;

struct FunctionIR {
  std::unordered_map<std::string_view, FuncParam> params;
  std::uint32_t localVarCount;
  std::vector<Operand> calleeRegs;
  IRListing insts;
};

struct IR {
  std::vector<FunctionIR> functions;
  IRListing globalInits;
};

struct RegAllocIR {
  std::vector<FunctionIR> functions;
  IRListing globalInits;
};
} // namespace Fanta
