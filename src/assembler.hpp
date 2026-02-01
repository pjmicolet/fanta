#pragma once 
#include <tuple>
#include <algorithm>
#include "instructions.hpp"

namespace Instructions {

struct Assembler {
  template<typename ...Inst>
  static constexpr auto compile() -> std::array<uint32_t, sizeof...(Inst)>{
    //Take all the insts, dump them in a tuple
    return {Inst::emit()...};
  }
};

template<typename ...Inst>
struct Program {
  static constexpr auto load() {
    return Assembler::compile<Inst...>();
  }
};
}
