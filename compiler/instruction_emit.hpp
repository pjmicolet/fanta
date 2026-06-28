#pragma once
#include "PassDefinition.hpp"
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Fanta {
struct InstructionEmitter {
  auto outputInstructions(RegAllocIR &rir, GlobalTable &gt) -> InstructionList;

private:
  auto outputInstructionsForFunc(const FunctionIR &fir, GlobalTable &gt,
                                 InstructionList &il) -> void;
  auto outputInitFunc(const FunctionIR &fir, GlobalTable &gt,
                      InstructionList &il) -> void;
  auto emitInst(const IROp &op, GlobalTable &gt, InstructionList &il) -> void;

  auto link(InstructionList &il) -> void;

  auto dummy() -> void;

  auto reset() -> void;

  using index = size_t;
  using funcName = std::string_view;
  using globalName = std::string_view;
  std::vector<std::pair<index, globalName>> missingLinks;
  std::unordered_map<funcName, uint32_t> resolvedAddresses;
  std::vector<index> globalBaseMovs;
};

} // namespace Fanta
