#pragma once

#include "PassDefinition.hpp"
#include "SimpleIRPass.hpp"
#include "codegen.hpp"
#include "instruction_emit.hpp"

namespace Fanta {

template <OutputsIR IRPass, OutputsRegAlloc RegAllocator,
          OutputsInstructions InstEmit>
struct CompilerDef {

  CompilerDef() {};

  auto compile(std::string program) {
    Parser p(program);
    p.fullWalk();
    Codegen cg{};
    cg.extractGlobalNames(p);
    Fanta::GlobalTable gt = cg.globalNameSpace;

    IRPass irPass{};
    auto virtualIR = irPass.outputIR(p, gt);

    RegAllocator ra{};
    auto allocatedIR = ra.outputIRWithRegs(virtualIR);

    InstEmit emitter{};
    auto insts = emitter.outputInstructions(allocatedIR, gt);

    return insts;
  }
};

using DefaultCompiler =
    CompilerDef<SimpleIRPass, Allocator, InstructionEmitter>;
} // namespace Fanta
