#pragma once
#include "cpu.hpp"
#include "trie.hpp"
#include "string_assembler.hpp"
#include <ncurses.h>
#include <string>
#include <vector>
#include <chrono>

enum class Mode {
    NORMAL,
    INSERT,
    ZOOM,
    COMMAND
};

class TUI {
public:
    TUI(CPU& cpu);
    ~TUI();

    void init_ncurses();
    void run();
    void dump_state(const std::string& filename);
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
    void handle_command_mode(int ch);

    CPU& cpu;
    bool running;
    uint32_t mem_offset = 0;
    bool is_running_continuously = false;
    
    int term_w = 80;
    int term_h = 24;

    // Editor state
    Mode mode = Mode::NORMAL;
    std::vector<std::string> editor_buffer;
    std::string command_buffer;
    int cursor_x = 0;
    int cursor_y = 0;
    int editor_scroll = 0;

    Assembler assem;
    Trie trie;
    void init_trie();

    std::array<std::uint32_t, 17> prev_registers{0};
    int last_changed_reg = -1;

    // IPS Monitoring
    uint64_t total_cycles = 0;
    double current_ips = 0;
    std::chrono::steady_clock::time_point last_ips_time;

    // Zooming
    double zoom_scale = 1.0;
    double zoom_cx = 160.0;
    double zoom_cy = 120.0;
};
