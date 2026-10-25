#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <print>
#include <array>
#include "cpu.hpp"
#include "program.hpp"

/**
 * fanta-diff: A high-signal tracer for the Fanta ISA.
 * Instead of dumping all state every cycle, it only prints what CHANGED.
 * 
 * Output Format:
 * Cycle [N] | PC 0xAAAA -> 0xBBBB
 *   Reg RX: 0xOLD -> 0xNEW
 *   Flag [F]: 0 -> 1
 *   Mem [Addr]: 0xOLD -> 0xNEW
 */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::println(std::cerr, "Usage: fanta-diff <filename.asm> [--limit N]");
        return 1;
    }

    std::string filename = argv[1];
    uint64_t limit = 5000;

    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--limit" && i + 1 < argc) {
            limit = std::stoull(argv[i + 1]);
            i++;
        }
    }

    CPU cpu;

    std::vector<Fanta::Program::AsmError> errors;
    if (!Fanta::Program::loadInto(cpu, filename, errors)) {
        std::println(std::cerr, "Error: Could not open file {}", filename);
        return 1;
    }
    for (const auto& e : errors) {
        std::println(std::cerr, "warning: line {} failed to assemble (NOP): {}", e.line, e.text);
    }

    // Shadow state for diffing
    std::array<uint32_t, 17> prev_regs = cpu.registers;
    std::array<uint8_t, 4> prev_flags = cpu.status_reg;

    std::println("--- START DIFF TRACE: {} ---", filename);
    
    uint64_t cycle = 0;
    while (!cpu.halted && cycle < limit) {
        uint32_t current_pc = cpu.get_pc();
        
        // Execute one cycle
        cpu.run_cycle();
        cycle++;

        bool header_printed = false;
        auto print_header = [&]() {
            if (!header_printed) {
                std::println("Cycle {:<5} | PC: 0x{:04X} -> 0x{:04X}", cycle-1, current_pc, cpu.get_pc());
                header_printed = true;
            }
        };

        // Diff Registers
        for (int i = 0; i < 17; ++i) {
            if (cpu.registers[i] != prev_regs[i]) {
                print_header();
                std::string label = (i == 16) ? "SP " : "R" + std::to_string(i);
                std::println("  {:<2}: 0x{:08X} -> 0x{:08X}", label, prev_regs[i], cpu.registers[i]);
                prev_regs[i] = cpu.registers[i];
            }
        }

        // Diff Flags
        static const char* flag_names[] = {"Z", "N", "V", "C"};
        for (int i = 0; i < 4; ++i) {
            if (cpu.status_reg[i] != prev_flags[i]) {
                print_header();
                std::println("  Flag {}: {} -> {}", flag_names[i], (int)prev_flags[i], (int)cpu.status_reg[i]);
                prev_flags[i] = cpu.status_reg[i];
            }
        }

        if (header_printed) {
            std::println("---------------------------------------");
        }
    }

    if (cpu.halted) std::println("CPU HALTED at cycle {}", cycle);
    return 0;
}
