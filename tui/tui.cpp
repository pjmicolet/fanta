#include "tui.hpp"
#include "instructions.hpp"
#include <iomanip>
#include <sstream>

TUI::TUI(CPU& cpu) : cpu(cpu), running(true) {
}

TUI::~TUI() {
    endwin();
}

void TUI::init_ncurses() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    timeout(100); // 100ms timeout for getch
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Registers
        init_pair(2, COLOR_GREEN, COLOR_BLACK);  // PC / Disassembly
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Status
        init_pair(4, COLOR_RED, COLOR_BLACK);    // Halted
    }
}

void TUI::run() {
    while (running) {
        update();
        handle_input();
    }
}

void TUI::update() {
    erase();
    draw_registers();
    draw_status();
    draw_disassembly();
    draw_memory();
    refresh();
}

void TUI::draw_registers() {
    attron(COLOR_PAIR(1));
    mvprintw(1, 2, "--- REGISTERS ---");
    for (int i = 0; i < 8; ++i) {
        mvprintw(2 + i, 2, "R%d: 0x%08X (%u)", i, cpu.registers[i], cpu.registers[i]);
    }
    attroff(COLOR_PAIR(1));
}

void TUI::draw_status() {
    attron(COLOR_PAIR(3));
    mvprintw(1, 30, "--- STATUS ---");
    mvprintw(2, 30, "Z: %d", cpu.status_reg[0]); // Zero
    mvprintw(3, 30, "N: %d", cpu.status_reg[1]); // Negative
    mvprintw(4, 30, "V: %d", cpu.status_reg[2]); // Overflow
    mvprintw(5, 30, "C: %d", cpu.status_reg[3]); // Carry
    
    if (cpu.halted) {
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(7, 30, "!!! HALTED !!!");
        attroff(COLOR_PAIR(4) | A_BOLD);
    } else {
        mvprintw(7, 30, "RUNNING...");
    }
    attroff(COLOR_PAIR(3));
}

void TUI::draw_disassembly() {
    attron(COLOR_PAIR(2));
    mvprintw(1, 50, "--- DISASSEMBLY ---");
    
    // We want to show some context around the current instruction.
    // If the CPU hasn't run yet, PC is 0. 
    // Let's use PC directly if it's 0, or PC-4 if it's already run.
    uint32_t current_pc = (cpu.halted || cpu.get_prev_pc() > 0x1000000) ? 0 : cpu.get_prev_pc();
    
    for (int i = 0; i < 10; ++i) {
        // Show 2 instructions before and 7 after current_pc
        int32_t addr = (int32_t)current_pc - 8 + (i * 4);
        if (addr < 0 || addr >= 16*1024*1024 - 4) {
            mvprintw(2 + i, 50, "0x%04X: <OUT OF BOUNDS>", (uint32_t)addr);
            continue;
        }

        std::string line = disassemble((uint32_t)addr);
        if ((uint32_t)addr == current_pc) {
            mvprintw(2 + i, 48, "> 0x%04X: %s", (uint32_t)addr, line.c_str());
        } else {
            mvprintw(2 + i, 50, "0x%04X: %s", (uint32_t)addr, line.c_str());
        }
    }
    attroff(COLOR_PAIR(2));
}

void TUI::draw_memory() {
    mvprintw(14, 2, "--- MEMORY (0x%04X) ---", mem_offset);
    uint32_t ram_size = 16*1024*1024;

    for (int i = 0; i < 8; ++i) {
        uint32_t row_addr = mem_offset + (i * 16);
        if (row_addr >= ram_size) break;

        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << row_addr << ": ";
        
        uint8_t* ram_ptr = cpu.ram.from(row_addr);
        for (int j = 0; j < 16; ++j) {
            if (row_addr + j >= ram_size) break;
            uint8_t val = ram_ptr[j];
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)val << " ";
        }
        mvprintw(15 + i, 2, "%s", ss.str().c_str());
    }
}

void TUI::handle_input() {
    int ch = getch();
    switch (ch) {
        case 'q':
            running = false;
            break;
        case 's': // Step
            if (!cpu.halted) cpu.run_cycle();
            break;
        case 'r': // Run (continuous)
            // TBD: Toggle run mode
            break;
        case KEY_UP:
            if (mem_offset >= 16) mem_offset -= 16;
            break;
        case KEY_DOWN:
            mem_offset += 16;
            break;
    }
}

std::string TUI::disassemble(uint32_t addr) {
    if (addr >= 16*1024*1024 - 4) return "OUT OF BOUNDS";
    // Basic disassembly logic based on Instructions::Decode
    uint32_t raw = cpu.ram.read32(addr);
    Instructions::Decode d(raw);
    uint8_t opcode = d.getOpcode();

    switch (opcode) {
        case 0x0: return "HALT";
        case 0x1: return "ADD R" + std::to_string(d.getDestSrc()) + ", R" + std::to_string(d.getS1()) + ", R" + std::to_string(d.getS2());
        case 0x2: return "ADD R" + std::to_string(d.getDestSrc()) + ", R" + std::to_string(d.getS1()) + ", #" + std::to_string(d.getImm());
        case 0x3: return "MOV R" + std::to_string(d.getDestSrc()) + ", R" + std::to_string(d.getS1());
        case 0x4: return "MOV R" + std::to_string(d.getDestSrc()) + ", #" + std::to_string(d.getImm());
        case 0x5: return "SUB R" + std::to_string(d.getDestSrc()) + ", R" + std::to_string(d.getS1()) + ", R" + std::to_string(d.getS2());
        case 0x6: return "SUB R" + std::to_string(d.getDestSrc()) + ", R" + std::to_string(d.getS1()) + ", #" + std::to_string(d.getImm());
        case 0x7: return "JMP 0x" + std::to_string(raw & 0x3FFFFFF);
        case 0x8: return "STORE R" + std::to_string(d.getDestSrc()) + ", [R" + std::to_string(d.getS1()) + " + #" + std::to_string(d.getImm()) + "]";
        case 0x9: return "LOAD R" + std::to_string(d.getS1()) + ", [R" + std::to_string(d.getDestSrc()) + " + #" + std::to_string(d.getImm()) + "]";
        case 0xA: return "BEQ 0x" + std::to_string(raw & 0x3FFFFFF);
        case 0xB: return "BNE 0x" + std::to_string(raw & 0x3FFFFFF);
        default: return "UNKNOWN (0x" + std::to_string(opcode) + ")";
    }
}
