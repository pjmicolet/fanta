#pragma once

#include "ast.hpp"
#include "ir.hpp"
#include "parser.hpp"
#include <concepts>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace Fanta {

struct GlobalFuncInfo {
  uint32_t address;
  std::string_view rt; // this is either the actual signed type of variable or
                       // the return type of function
  Fanta::AST::NodeIndex idx;
  size_t numParams;
  std::vector<std::string_view> paramTypes;
};

struct GlobalVarInfo {
  uint32_t offsetFromBase;
  uint32_t address;
  std::string_view rt; // this is either the actual signed type of variable or
                       // the return type of function
};

using TempReg = std::uint16_t;

struct LocalVarInfo {
  TempReg tr;
  std::string_view type;
};

struct LocalTable {

  auto allocateAnonymous() -> TempReg { return numberOfTemps++; }

  auto allocateNamed(std::string_view vname, std::string_view type) -> TempReg {
    auto current = numberOfTemps;
    namedVars[vname] = {current, type};
    return numberOfTemps++;
  }

  std::unordered_map<std::string_view, LocalVarInfo> namedVars;

private:
  TempReg numberOfTemps;
};

using GlobalNameInfo = std::variant<GlobalFuncInfo, GlobalVarInfo>;
using GlobalTable = std::unordered_map<std::string_view, GlobalNameInfo>;

using Instruction = std::uint32_t;
using InstructionList = std::vector<Instruction>;

template <typename T>
concept OutputsIR = requires(T a, Parser &p, GlobalTable &gt) {
  { a.outputIR(p, gt) } -> std::same_as<IR>;
};

template <typename T>
concept OutputsRegAlloc = requires(T a, IR &ir) {
  { a.outputIRWithRegs(ir) } -> std::same_as<RegAllocIR>;
};

template <typename T>
concept OutputsInstructions = requires(T a, RegAllocIR &ir, GlobalTable &gt) {
  { a.outputInstructions(ir, gt) } -> std::same_as<InstructionList>;
};
} // namespace Fanta
