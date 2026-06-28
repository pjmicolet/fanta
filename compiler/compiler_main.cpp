#include "../tui/disasm.hpp"
#include "SimpleIRPass.hpp"
#include "allocator.hpp"
#include "codegen.hpp"
#include "instruction_emit.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

auto main(int argc, char *argv[]) -> int {
  if (argc < 2) {
    std::println(
        std::cerr,
        "Usage: {} <input_file> [-o <output_file>] [--dump-ast] [--dump-ir]",
        argv[0]);
    return 1;
  }

  std::string inputFile = argv[1];
  std::string outputFile = "output.bin";
  bool dumpAST = false;
  bool dumpIR = false;
  bool disasm = false;

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-o" && i + 1 < argc) {
      outputFile = argv[i + 1];
      ++i;
    } else if (arg == "--dump-ast") {
      dumpAST = true;
    } else if (arg == "--dump-ir") {
      dumpIR = true;
    } else if (arg == "--disasm")
      disasm = true;
  }

  std::ifstream in(inputFile);
  if (!in) {
    std::println(std::cerr, "Error: Could not open input file '{}'", inputFile);
    return 1;
  }

  std::string code((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());

  try {
    // 1. Parsing
    Parser p{code};
    std::println("About to get stuck");
    p.fullWalk();

    if (dumpAST) {
      std::println("=== AST DUMP ===");
      p.dumpNodes();
    }

    // 2. Global Namespace Extraction
    Fanta::Codegen cg{};
    cg.extractGlobalNames(p);
    Fanta::GlobalTable gt = cg.globalNameSpace;

    // 3. Lowering to Virtual IR
    Fanta::SimpleIRPass pass;
    Fanta::IR virtualIR = pass.outputIR(p, gt);

    if (dumpIR) {
      std::println("=== VIRTUAL IR DUMP ===");
      Fanta::printIR(virtualIR);
    }

    // 4. Register Allocation & Stack Spilling
    Fanta::Allocator alloc{};
    Fanta::RegAllocIR rir;
    for (const auto &func : virtualIR.functions) {
      rir.functions.push_back(alloc.assignFunc(func));
    }

    // 5. Instruction Emission & Address Resolution
    Fanta::InstructionEmitter emitter{};
    auto insts = emitter.outputInstructions(rir, gt);

    if (insts.empty()) {
      std::println(std::cerr,
                   "Error: Compiled program yielded 0 instructions.");
      return 1;
    }

    if (disasm) {
      std::println("{}", Fanta::disassembleProgram(insts));
    }

    // 6. Write machine instructions to output binary
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) {
      std::println(std::cerr, "Error: Could not open output file '{}'",
                   outputFile);
      return 1;
    }
    out.write(reinterpret_cast<const char *>(insts.data()),
              insts.size() * sizeof(uint32_t));

    std::println("Successfully compiled: {} -> {} ({} instructions, {} bytes)",
                 inputFile, outputFile, insts.size(),
                 insts.size() * sizeof(uint32_t));

  } catch (const std::exception &e) {
    std::println(std::cerr, "Compiler Error: {}", e.what());
    return 1;
  }

  return 0;
}
