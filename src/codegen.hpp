#pragma once

#include "parser.hpp"
#include <unordered_map>
#include <string_view>
#include <variant>

namespace Fanta {

struct GlobalFuncInfo {
  uint32_t address;
  std::string_view rt; //this is either the actual signed type of variable or the return type of function
  Fanta::AST::NodeIndex idx;
  size_t numParams;
  std::vector<std::string_view> paramTypes;
};

struct GlobalVarInfo {
  uint32_t address;
  std::string_view rt; //this is either the actual signed type of variable or the return type of function
};

using GlobalNameInfo = std::variant<GlobalFuncInfo, GlobalVarInfo>;

struct Codegen {

  auto extractGlobalNames(const Parser& p) -> void;

  std::unordered_map<std::string_view, GlobalNameInfo> globalNameSpace;
};

}
