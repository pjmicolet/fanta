#pragma once
#include "cpu.hpp"
#include <ncurses.h>
#include <string>
#include <vector>

class TUI {
public:
    TUI(CPU& cpu);
    ~TUI();

    void init_ncurses();
    void run();
    std::string disassemble(uint32_t addr);

private:
    void update();
    void draw_registers();
    void draw_memory();
    void draw_disassembly();
    void draw_status();
    void handle_input();

    CPU& cpu;
    bool running;
    uint32_t mem_offset = 0;
};
