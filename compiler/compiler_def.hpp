#pragma once

#include "PassDefinition.hpp"
#include "codegen.hpp"

namespace Fanta {

template <OutputsIR IRPass, OutputsRegAlloc RegAllocator,
          OutputsInstructions InstEmit>
struct CompilerDef {

  CompilerDef() {};

  auto compile(std::string program) {
    p.resetProgram(program);
    p.fullWalk();
    Codegen cg{};
    cg.extractGlobalNames(p);

    IRPass irPass{};
    auto virtualIR = irPass.outputIR(p, gt);

    RegAllocator ra{};
    auto allocatedIR = ra.outputIRWithRegs(virtualIR);

    InstEmit emitter{};
    auto insts = emitter.outputInstructions(allocatedIR, gt);

    return insts;
  }

  Parser p;
  GlobalTable gt;
};
} // namespace Fanta
