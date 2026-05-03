#pragma once
#include "cpu.hpp"
#include "trie.hpp"
#include <ncurses.h>
#include <string>
#include <vector>

enum class Mode {
    NORMAL,
    INSERT
};

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
    void draw_editor();
    void draw_vram_preview();
    void handle_input();
    
    void handle_normal_mode(int ch);
    void handle_insert_mode(int ch);

    CPU& cpu;
    bool running;
    uint32_t mem_offset = 0;
    bool is_running_continuously = false;
    
    // Editor state
    Mode mode = Mode::NORMAL;
    std::vector<std::string> editor_buffer;
    int cursor_x = 0;
    int cursor_y = 0;
    int editor_scroll = 0;

    Trie trie;
    void init_trie();
};
