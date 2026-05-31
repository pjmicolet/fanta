#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <print>
#include "cpu.hpp"
#include "string_assembler.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println(std::cerr, "Usage: fanta-trace <filename.asm> [--limit N]");
        return 1;
    }

    std::string filename = argv[1];
    uint64_t limit = 1000;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--limit" && i + 1 < argc) {
            limit = std::stoull(argv[i + 1]);
            i++;
        }
    }

    std::ifstream in(filename);
    if (!in) {
        std::println(std::cerr, "Error: Could not open file {}", filename);
        return 1;
    }

    std::vector<std::string> lines;
    std::string line;
    std::string full_code;
    while (std::getline(in, line)) {
        lines.push_back(line);
        full_code += line + "\n";
    }

    CPU cpu;
    Assembler assem;

    // Pass 1: Labels
    assem.scan_for_labels(full_code);

    // Pass 2: Assemble and load
    for (size_t i = 0; i < lines.size(); ++i) {
        uint32_t instr = assem.assemble(lines[i], i * 4);
        if (instr != (uint32_t)-1) {
            cpu.store(i * 4, instr);
        } else {
            // NOP for labels/empty lines
            cpu.store(i * 4, 0x14 << 26);
        }
    }

    std::println("--- START TRACE: {} ---", filename);
    
    uint64_t cycle = 0;
    while (!cpu.halted && cycle < limit) {
        uint32_t pc = cpu.get_pc();
        uint32_t sp = cpu.get_sp();
        uint32_t raw = cpu.ram.read32(pc);

        std::println("Cycle: {:<5} | PC: 0x{:04X} | SP: 0x{:04X} | Inst: 0x{:08X}", cycle, pc, sp, raw);
        
        // Print registers in 2 rows of 8
        std::print("  ");
        for (int i = 0; i < 8; ++i) {
            std::print("R{}: 0x{:08X} ", i, cpu.registers[i]);
        }
        std::println("");
        std::print("  ");
        for (int i = 8; i < 16; ++i) {
            std::print("R{:<2}: 0x{:08X} ", i, cpu.registers[i]);
        }
        std::println("");

        // Flags
        std::println("  Flags: Z:{} N:{} V:{} C:{}", 
            cpu.status_reg[0], cpu.status_reg[1], cpu.status_reg[2], cpu.status_reg[3]);
        std::println("------------------------------------------------------------------");

        cpu.run_cycle();
        cycle++;
    }

    if (cpu.halted) {
        std::println("CPU HALTED at cycle {}", cycle);
    } else if (cycle >= limit) {
        std::println("Trace reached instruction limit ({})", limit);
    }

    return 0;
}
