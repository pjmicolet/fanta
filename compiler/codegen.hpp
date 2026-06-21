#pragma once

#include "parser.hpp"
#include <string_view>
#include <utility>
#include "PassDefinition.hpp"

namespace Fanta {

struct Codegen {

  auto extractGlobalNames(const Parser& p) -> void;

  // Go through every root and generate the code
  // populate the GlobalNameInfo as we go
  // When we don't know where a symbol is (but know it exists) we add to missingLinks (address and symbol)
  auto generate(const Parser& p) -> void;
  
  //Finish linking the calls
  auto link() -> void;

  GlobalTable globalNameSpace;
  std::vector<std::pair<std::uint32_t, std::string_view>> missingLinks;
};

}
