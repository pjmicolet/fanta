#pragma once

#include <expected>
#include <utility>

#include "PassDefinition.hpp"
#include "SimpleIRPass.hpp"
#include "codegen.hpp"
#include "instruction_emit.hpp"

namespace Fanta {

template <OutputsIR IRPass, OutputsRegAlloc RegAllocator,
          OutputsInstructions InstEmit>
struct CompilerDef {

  CompilerDef() = default;

  auto compile(std::string program)
      -> std::expected<InstructionList, ParserError> {
    Parser p(std::move(program));
    auto res = p.fullWalk();
    if (!res)
      return std::unexpected(ParserError{res.error()});
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
