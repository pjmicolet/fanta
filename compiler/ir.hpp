#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Fanta {
using InstOp = std::uint16_t; // Opcode

struct Operand {
  uint32_t val;
  bool isVirtual;
};

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
  std::optional<Operand> dest; // some calls are void
};

// Useful for if statements, we add a label after the instructions of the if
// body so that we can later calculate the jump
struct IRLabel {
  std::string name;
};

struct LocalGlobalBase {
  Operand dest;
};

using IRInst = std::variant<IROp, CallFunc, LocalGlobalBase, IRLabel>;

using IRListing = std::vector<IRInst>;

struct FunctionIR {
  std::unordered_map<std::string_view, FuncParam> params;
  std::uint32_t localVarCount;
  std::vector<Operand> calleeRegs;
  IRListing insts;
  std::string_view name;
};

struct IR {
  std::vector<FunctionIR> functions;
  IRListing globalInits;
};

struct RegAllocIR {
  std::vector<FunctionIR> functions;
  IRListing globalInits;
};

auto printIR(const IR &irlisting) -> void;

} // namespace Fanta
