#include "tui.hpp"
#include "instructions.hpp"
#include <cassert>
#include <iostream>
#include <vector>

void test_disassembler() {
    CPU cpu{};
    TUI tui(cpu);

    // Test HALT (Opcode 0)
    cpu.ram.write32(0, 0); 
    std::string res = tui.disassemble(0);
    std::cout << "HALT: " << res << std::endl;
    assert(res == "HALT");

    // Test ADD R1, R2, R3 (Opcode 1)
    // format: [opcode:6bit][dest:5bit][src1:5bit][src2:5bit]
    // 0x1 << 26 | 1 << 21 | 2 << 16 | 3 << 11
    uint32_t add_instr = (0x1 << 26) | (1 << 21) | (2 << 16) | (3 << 11);
    cpu.ram.write32(4, add_instr);
    res = tui.disassemble(4);
    std::cout << "ADD: " << res << std::endl;
    assert(res == "ADD R1, R2, R3");

    // Test CIP #0 (Opcode 0x1F)
    uint32_t cip_instr = (0x1F << 26) | 0;
    cpu.ram.write32(8, cip_instr);
    res = tui.disassemble(8);
    std::cout << "CIP: " << res << std::endl;
    assert(res == "CIP #0");
}

void test_bounds() {
    CPU cpu{};
    // Manually test the logic we use in draw_disassembly
    uint32_t pc = 0;
    uint32_t prev_pc = pc - 4;
    
    std::cout << "Testing PC underflow detection (0 - 4 = 0x" << std::hex << prev_pc << ")..." << std::endl;
    bool is_underflow = (prev_pc > 16*1024*1024); 
    assert(is_underflow == true);
    
    TUI tui(cpu);
    std::string res = tui.disassemble(16*1024*1024 - 4);
    assert(res == "OUT OF BOUNDS");
}

int main() {
    try {
        test_bounds();
        test_disassembler();
        std::cout << "All TUI unit tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
