#include "tui.hpp"
#include "instructions.hpp"
#include "string_assembler.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

TUI::TUI(CPU& cpu) : cpu(cpu), running(true) {
    editor_buffer.push_back("");
    init_trie();
}

void TUI::init_trie() {
    for (auto const& [mnem, meta] : Instructions::Registry::get()) {
        trie.insert(mnem);
    }
}

TUI::~TUI() {
    endwin();
}

void TUI::init_ncurses() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1); // Show cursor for editor
    timeout(50); // 50ms delay for continuous mode feel
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Registers
        init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Disassembly / Normal Mode
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Status / Insert Mode
        init_pair(4, COLOR_RED, COLOR_BLACK);    // Halted
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Memory
        init_pair(6, COLOR_WHITE, COLOR_BLACK);  // VRAM preview
    }
}

void TUI::run() {
    while (running) {
        if (is_running_continuously) {
            if (!cpu.halted) {
                cpu.run_cycle();
            } else {
                is_running_continuously = false;
            }
        }
        update();
        handle_input();
    }
}

void TUI::update() {
    getmaxyx(stdscr, term_h, term_w);
    erase();
    draw_registers();
    draw_status();
    draw_disassembly();
    draw_memory();
    draw_editor();
    draw_vram_preview();
    
    // Position cursor in the editor window relative to its starting Y
    int editor_y = term_h * 0.6;
    move(editor_y + (cursor_y - editor_scroll), 6 + cursor_x);
    
    refresh();
}

void TUI::draw_registers() {
    attron(COLOR_PAIR(1));
    mvprintw(1, 2, "--- REGISTERS ---");
    for (int i = 0; i < 8; ++i) {
        if (2 + i < term_h)
            mvprintw(2 + i, 2, "R%d: 0x%08X", i, cpu.registers[i]);
    }
    attroff(COLOR_PAIR(1));
}

void TUI::draw_status() {
    attron(COLOR_PAIR(3));
    int col = term_w * 0.25;
    if (col < 20) col = 20; // Minimum column offset
    mvprintw(1, col, "--- STATUS ---");
    mvprintw(2, col, "Z:%d N:%d V:%d C:%d", cpu.status_reg[0], cpu.status_reg[1], cpu.status_reg[2], cpu.status_reg[3]);
    mvprintw(3, col, "MODE: %s %s", (mode == Mode::NORMAL ? "NORMAL" : "INSERT"), (is_running_continuously ? "(CONTINUOUS)" : ""));
    mvprintw(4, col, "KEYS: %s", (mode == Mode::NORMAL ? "s:step c:cont i:edit q:quit" : "ESC:exit"));
    
    if (cpu.halted) {
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(5, col, "!!! HALTED !!!");
        attroff(COLOR_PAIR(4) | A_BOLD);
    }
    attroff(COLOR_PAIR(3));
}

void TUI::draw_disassembly() {
    attron(COLOR_PAIR(2));
    int col = term_w * 0.55;
    if (col < 40) col = 40;
    mvprintw(1, col, "--- DISASSEMBLY ---");
    uint32_t current_pc = (cpu.halted || cpu.get_prev_pc() > 16*1024*1024) ? 0 : cpu.get_prev_pc();
    
    int max_lines = (term_h * 0.5) - 2;
    if (max_lines < 5) max_lines = 5;

    for (int i = 0; i < max_lines; ++i) {
        int32_t addr = (int32_t)current_pc - (max_lines/2 * 4) + (i * 4);
        if (addr < 0 || addr >= 16*1024*1024 - 4) continue;
        if (2 + i >= term_h * 0.6) break;

        std::string line = disassemble((uint32_t)addr);
        if ((uint32_t)addr == current_pc) {
            mvprintw(2 + i, col - 2, "> 0x%04X: %s", (uint32_t)addr, line.c_str());
        } else {
            mvprintw(2 + i, col, "0x%04X: %s", (uint32_t)addr, line.c_str());
        }
    }
    attroff(COLOR_PAIR(2));
}

void TUI::draw_memory() {
    attron(COLOR_PAIR(5));
    int start_y = term_h * 0.4;
    if (start_y < 10) start_y = 10;
    
    mvprintw(start_y, 2, "--- MEMORY (0x%04X) ---", mem_offset);
    for (int i = 0; i < 4; ++i) {
        uint32_t row_addr = mem_offset + (i * 16);
        if (row_addr >= 16*1024*1024 || start_y + 1 + i >= term_h * 0.6) break;
        std::stringstream ss;
        ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << row_addr << ": ";
        uint8_t* ptr = cpu.ram.from(row_addr);
        for (int j = 0; j < 16; ++j) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)ptr[j] << " ";
        }
        mvprintw(start_y + 1 + i, 2, "%s", ss.str().c_str());
    }
    attroff(COLOR_PAIR(5));
}

void TUI::draw_editor() {
    int win_y = term_h * 0.6;
    int win_h = term_h - win_y - 1;
    if (win_h < 3) win_h = 3;
    
    attron(A_BOLD);
    mvprintw(win_y - 1, 2, "--- EDITOR (Mnemonic Entry) ---");
    attroff(A_BOLD);
    
    for (int i = 0; i < win_h; ++i) {
        int line_idx = editor_scroll + i;
        if (line_idx < (int)editor_buffer.size()) {
            std::string line = editor_buffer[line_idx];
            mvprintw(win_y + i, 2, "%2d| %s", line_idx, line.c_str());
            
            // Auto-suggestion for the current line being edited
            if (line_idx == cursor_y && mode == Mode::INSERT && !line.empty()) {
                // If there's no space yet, suggest the mnemonic
                if (line.find(' ') == std::string::npos) {
                    std::string suggestion = trie.get_suggestion(line);
                    if (!suggestion.empty() && suggestion.size() > line.size()) {
                        attron(A_DIM);
                        printw("%s", suggestion.substr(line.size()).c_str());
                        attroff(A_DIM);
                        
                        // Show format suggestion
                        auto& meta = Instructions::Registry::fetch(suggestion);
                        attron(COLOR_PAIR(3));
                        std::string fmt_hint;
                        switch(meta.fmt) {
                            case Instructions::THREE_OP: fmt_hint = " R, R, R/#"; break;
                            case Instructions::TWO_OP: fmt_hint = " R, R/#"; break;
                            case Instructions::MEM: fmt_hint = " R, #imm(R)"; break;
                            case Instructions::JUMP: fmt_hint = " #imm"; break;
                            case Instructions::BRANCH: fmt_hint = " #imm"; break;
                            case Instructions::HALT: fmt_hint = ""; break;
                        }
                        mvprintw(win_y + i, term_w * 0.4, "(%s)", fmt_hint.c_str());
                        attroff(COLOR_PAIR(3));
                    }
                }
            }
        } else {
            mvprintw(win_y + i, 2, "%2d|", line_idx);
        }
    }
}

void TUI::draw_vram_preview() {
    int start_col = term_w * 0.55;
    int start_y = term_h * 0.6;
    int preview_w = term_w - start_col - 4;
    int preview_h = term_h - start_y - 1;
    
    if (preview_w <= 0 || preview_h <= 0) return;

    mvprintw(start_y - 1, start_col, "--- VRAM PREVIEW ---");
    uint32_t* vram = (uint32_t*)cpu.get_vram();
    
    for (int y = 0; y < preview_h; ++y) {
        for (int x = 0; x < preview_w; ++x) {
            float scale_x = 320 / (float)preview_w;
            float scale_y = 240 / (float)preview_h;
            int vx = x * scale_x;
            int vy = y * scale_y;
            if (vx >= 320 || vy >= 240) continue;
            
            uint32_t pixel = vram[vy * 320 + vx];
            if (pixel != 0xFF000000 && pixel != 0) {
                mvaddch(start_y + y, start_col + x, '#');
            } else {
                mvaddch(start_y + y, start_col + x, '.');
            }
        }
    }
}

void TUI::handle_input() {
    int ch = getch();
    if (ch == ERR) return;

    if (ch == 27) { // ESC always stops continuous run
        is_running_continuously = false;
    }

    if (mode == Mode::NORMAL) {
        handle_normal_mode(ch);
    } else {
        handle_insert_mode(ch);
    }
}

void TUI::handle_normal_mode(int ch) {
    switch (ch) {
        case 'q': running = false; break;
        case 'i': mode = Mode::INSERT; curs_set(1); is_running_continuously = false; break;
        case 's': if (!cpu.halted) cpu.run_cycle(); break;
        case 'c': if (!cpu.halted) is_running_continuously = !is_running_continuously; break;
        case KEY_UP: if (cursor_y > 0) cursor_y--; break;
        case KEY_DOWN: if (cursor_y < (int)editor_buffer.size() - 1) cursor_y++; break;
        case KEY_LEFT: if (cursor_x > 0) cursor_x--; break;
        case KEY_RIGHT: if (cursor_x < (int)editor_buffer[cursor_y].size()) cursor_x++; break;
        case 'x': // Delete character
            if (cursor_x < (int)editor_buffer[cursor_y].size()) {
                editor_buffer[cursor_y].erase(cursor_x, 1);
            }
            break;
    }
}

void TUI::handle_insert_mode(int ch) {
    if (ch == 27) { // ESC
        mode = Mode::NORMAL;
        curs_set(0);
        return;
    }

    switch (ch) {
        case '\t': { // TAB completion
            std::string line = editor_buffer[cursor_y];
            if (line.find(' ') == std::string::npos && !line.empty()) {
                std::string suggestion = trie.get_suggestion(line);
                if (!suggestion.empty()) {
                    editor_buffer[cursor_y] = suggestion + " ";
                    cursor_x = editor_buffer[cursor_y].size();
                }
            }
            break;
        }
        case KEY_BACKSPACE:
        case 127:
            if (cursor_x > 0) {
                editor_buffer[cursor_y].erase(cursor_x - 1, 1);
                cursor_x--;
            } else if (cursor_y > 0) {
                cursor_x = editor_buffer[cursor_y - 1].size();
                editor_buffer[cursor_y - 1] += editor_buffer[cursor_y];
                editor_buffer.erase(editor_buffer.begin() + cursor_y);
                cursor_y--;
            }
            break;
        case KEY_ENTER:
        case '\n': {
            // New line insertion logic
            std::string remaining = editor_buffer[cursor_y].substr(cursor_x);
            editor_buffer[cursor_y] = editor_buffer[cursor_y].substr(0, cursor_x);
            editor_buffer.insert(editor_buffer.begin() + cursor_y + 1, remaining);
            cursor_y++;
            cursor_x = 0;

            // Two-pass assembly for the entire buffer
            try {
                // Pass 1: Scan for labels
                std::string full_code;
                for (const auto& line : editor_buffer) {
                    full_code += line + "\n";
                }
                assem.scan_for_labels(full_code);

                // Pass 2: Re-assemble all lines into memory
                for (size_t i = 0; i < editor_buffer.size(); ++i) {
                    if (editor_buffer[i].empty()) continue;
                    uint32_t instr = assem.assemble(editor_buffer[i], i * 4);
                    if (instr != (uint32_t)-1) {
                        cpu.store(i * 4, instr);
                    }
                }
            } catch (...) {
                // Ignore errors during re-assembly
            }
            break;
        }
        default:
            if (isprint(ch)) {
                editor_buffer[cursor_y].insert(cursor_x, 1, ch);
                cursor_x++;
            }
            break;
    }
}

std::string TUI::disassemble(uint32_t addr) {
    if (addr >= 16*1024*1024 - 4) return "OUT OF BOUNDS";
    uint32_t raw = cpu.ram.read32(addr);
    Instructions::Decode d(raw);
    uint8_t opcode = d.getOpcode();

    std::string label = assem.get_label_for((int)addr);
    std::string label_tag = label.empty() ? "" : "[" + label + "] ";

    // Mapping of opcode to name for display
    static std::unordered_map<uint8_t, std::string> names;
    if (names.empty()) {
        for (auto& [mnem, meta] : Instructions::Registry::get()) {
            names[meta.reg] = mnem;
            if (meta.imm != 0) names[meta.imm] = mnem;
        }
        names[0] = "HALT";
    }

    if (names.find(opcode) == names.end()) return "UNKNOWN";
    
    std::string name = names[opcode];
    auto& mtd = Instructions::Registry::fetch(name);
    
    std::stringstream ss;
    ss << name << " ";
    switch(mtd.fmt) {
        case Instructions::THREE_OP:
            ss << "R" << (int)d.getDestSrc() << ", R" << (int)d.getS1() << ", ";
            if (opcode == mtd.imm) {
                ss << "#" << std::hex << (int)d.getImm();
            } else {
                ss << "R" << (int)d.getS2();
            }
            break;
        case Instructions::TWO_OP:
            ss << "R" << (int)d.getDestSrc() << ", ";
            if (opcode == mtd.imm) {
                ss << "#" << std::hex << (int)d.getImm();
            } else {
                ss << "R" << (int)d.getS1();
            }
            break;
        case Instructions::MEM:
            ss << "R" << (int)d.getDestSrc() << ", " << std::hex << (int)d.getImm() << "(R" << std::dec << (int)d.getS1() << ")";
            break;
        case Instructions::JUMP:
        case Instructions::BRANCH: {
            int32_t offset = (int32_t)(raw & 0x3FFFFFF);
            // Sign extend 26-bit to 32-bit
            if (offset & 0x2000000) offset |= 0xFC000000;
            
            // For JMP (absolute) vs Branch (relative)
            uint32_t target = (opcode == Instructions::Registry::fetch("JMP").reg) ? (raw & 0x3FFFFFF) : (addr + offset);
            
            std::string target_label = assem.get_label_for((int)target);
            if (!target_label.empty()) {
                ss << target_label << " <0x" << std::hex << target << ">";
            } else {
                ss << "0x" << std::hex << target;
            }
            
            if (opcode != Instructions::Registry::fetch("JMP").reg) {
                ss << " (offset: " << std::dec << offset << ")";
            }
            break;
        }
        case Instructions::HALT:
            return label_tag + "HALT";
    }
    return label_tag + ss.str();
}
