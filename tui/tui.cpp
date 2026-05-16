#include "tui.hpp"
#include "instructions.hpp"
#include "string_assembler.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>

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
        init_pair(6, COLOR_WHITE, COLOR_BLACK);  // VRAM preview (default)

        // Initialize 6x6x6 color cube starting at pair 10
        // xterm colors 16-231 are the 6x6x6 cube
        if (COLORS >= 256) {
            for (int r = 0; r < 6; ++r) {
                for (int g = 0; g < 6; ++g) {
                    for (int b = 0; b < 6; ++b) {
                        int color_idx = 16 + (r * 36) + (g * 6) + b;
                        int pair_idx = 10 + (r * 36) + (g * 6) + b;
                        init_pair(pair_idx, color_idx, color_idx); // Same fg/bg for solid block
                    }
                }
            }
        }
    }
}

void TUI::run() {
    while (running) {
        if (is_running_continuously) {
            // Run a batch of cycles to speed up execution
            for (int i = 0; i < 1000; ++i) {
                if (!cpu.halted) {
                    prev_registers = cpu.registers;
                    cpu.run_cycle();
                    
                    // Update last_changed_reg for the very last instruction in the batch
                    if (i == 999) {
                        last_changed_reg = -1;
                        for (int r = 0; r < 8; ++r) {
                            if (cpu.registers[r] != prev_registers[r]) {
                                last_changed_reg = r;
                                break;
                            }
                        }
                    }
                } else {
                    is_running_continuously = false;
                    break;
                }
            }
        }
        update();
        handle_input();
    }
}

void TUI::update() {
    getmaxyx(stdscr, term_h, term_w);
    erase();

    if (mode == Mode::ZOOM) {
        draw_vram_preview();
        refresh();
        return;
    }

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
    attroff(COLOR_PAIR(1));

    for (int i = 0; i < 8; ++i) {
        if (2 + i < term_h) {
            if (i == last_changed_reg) {
                attron(COLOR_PAIR(3) | A_BOLD);
                mvprintw(2 + i, 2, "R%d: 0x%08X", i, cpu.registers[i]);
                attroff(COLOR_PAIR(3) | A_BOLD);
            } else {
                attron(COLOR_PAIR(1));
                mvprintw(2 + i, 2, "R%d: 0x%08X", i, cpu.registers[i]);
                attroff(COLOR_PAIR(1));
            }
        }
    }
}

void TUI::draw_status() {
    attron(COLOR_PAIR(3));
    int col = term_w * 0.25;
    if (col < 20) col = 20; 
    mvprintw(1, col, "--- STATUS ---");
    mvprintw(2, col, "PC: 0x%04X", cpu.get_pc());
    mvprintw(3, col, "FLAGS: Z:%d N:%d V:%d C:%d", cpu.status_reg[0], cpu.status_reg[1], (cpu.status_reg[2] ? 1 : 0), (cpu.status_reg[3] ? 1 : 0));
    mvprintw(4, col, "MODE: %s %s", (mode == Mode::NORMAL ? "NORMAL" : (mode == Mode::INSERT ? "INSERT" : (mode == Mode::ZOOM ? "ZOOM" : "COMMAND"))), (is_running_continuously ? "(CONT)" : ""));
    mvprintw(5, col, "KEYS: %s", (mode == Mode::NORMAL ? "s:step c:cont r:reset i:edit q:quit" : "ESC:exit"));
    
    if (!command_buffer.empty()) {
        attron(A_REVERSE);
        mvprintw(term_h - 1, 0, "%s", command_buffer.c_str());
        attroff(A_REVERSE);
    }

    if (cpu.halted) {
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(6, col, "!!! HALTED !!!");
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
    int start_col, start_y, preview_w, preview_h;

    if (mode == Mode::ZOOM) {
        start_col = 0;
        start_y = 0;
        preview_w = term_w;
        preview_h = term_h;
    } else {
        start_col = term_w * 0.55;
        start_y = term_h * 0.6;
        preview_w = term_w - start_col - 4;
        preview_h = term_h - start_y - 1;
        mvprintw(start_y - 1, start_col, "--- VRAM MONITOR ---");
    }
    
    if (preview_w <= 0 || preview_h <= 0) return;

    uint32_t* vram = (uint32_t*)cpu.get_vram();
    bool use_color = (COLORS >= 256);

    for (int y = 0; y < preview_h; ++y) {
        for (int x = 0; x < preview_w; ++x) {
            int vx = (x * 320) / preview_w;
            int vy = (y * 240) / preview_h;
            
            if (vx >= 320 || vy >= 240) continue;
            
            uint32_t pixel = vram[vy * 320 + vx];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;

            if (use_color) {
                int r5 = (r * 5) / 255;
                int g5 = (g * 5) / 255;
                int b5 = (b * 5) / 255;
                int pair_idx = 10 + (r5 * 36) + (g5 * 6) + b5;
                
                attron(COLOR_PAIR(pair_idx));
                mvaddch(start_y + y, start_col + x, ' '); 
                attroff(COLOR_PAIR(pair_idx));
            } else {
                const char* ramp = " .:-=+*#%@";
                uint8_t lum = (r + g + b) / 3;
                int char_idx = (lum * 9) / 255;
                mvaddch(start_y + y, start_col + x, ramp[char_idx]);
            }
        }
    }
}

void TUI::handle_input() {
    int ch = getch();
    if (ch == ERR) return;

    if (ch == 27) { // ESC
        is_running_continuously = false;
        if (mode == Mode::ZOOM) {
            mode = Mode::NORMAL;
            return;
        }
    }

    if (mode == Mode::NORMAL) {
        handle_normal_mode(ch);
    } else if (mode == Mode::INSERT) {
        handle_insert_mode(ch);
    } else if (mode == Mode::ZOOM) {
        // In ZOOM mode, we still want to step, reset, and continue
        switch(ch) {
            case 's': if (!cpu.halted) cpu.run_cycle(); break;
            case 'c': if (!cpu.halted) is_running_continuously = !is_running_continuously; break;
            case 'r': handle_normal_mode('r'); break; // Reuse reset logic
            case 'z': mode = Mode::NORMAL; break;
        }
    } else if (mode == Mode::COMMAND) {
        handle_command_mode(ch);
    }
}

void TUI::handle_normal_mode(int ch) {
    switch (ch) {
        case 'q': running = false; break;
        case ':':
            mode = Mode::COMMAND;
            command_buffer = ":";
            is_running_continuously = false;
            break;
        case 'z': mode = Mode::ZOOM; is_running_continuously = false; break;
        case 'r': // Full CPU Reset
            cpu.set_pc(0);
            cpu.halted = false;
            cpu.registers.fill(0);
            cpu.status_reg.fill(0);
            prev_registers.fill(0);
            last_changed_reg = -1;
            is_running_continuously = false;
            // Optionally clear first 1MB of RAM if you want a "clean" run
            for (uint32_t i = 0; i < 1024*1024; i += 4) {
                cpu.store(i, 0);
            }
            // Clear VRAM (320x240 * 4 bytes = 307,200 bytes)
            for (uint32_t i = 0x800000; i < 0x800000 + (320*240*4); i += 4) {
                cpu.store(i, 0);
            }
            // Re-assemble current buffer back into the clean memory
            for (size_t i = 0; i < editor_buffer.size(); ++i) {
                uint32_t instr = assem.assemble(editor_buffer[i], i * 4);
                if (instr != (uint32_t)-1) {
                    cpu.store(i * 4, instr);
                } else {
                    cpu.store(i * 4, 0x14 << 26); // Official NOP
                }
            }
            break;
        case 'i': mode = Mode::INSERT; curs_set(1); is_running_continuously = false; break;
        case 's': 
            if (!cpu.halted) {
                prev_registers = cpu.registers;
                cpu.run_cycle();
                last_changed_reg = -1;
                for (int i = 0; i < 8; ++i) {
                    if (cpu.registers[i] != prev_registers[i]) {
                        last_changed_reg = i;
                        break;
                    }
                }
            }
            break;
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
        
        // Apply changes to the whole buffer when exiting insert mode
        try {
            std::string full_code;
            for (const auto& line : editor_buffer) {
                full_code += line + "\n";
            }
            assem.scan_for_labels(full_code);

            for (size_t i = 0; i < editor_buffer.size(); ++i) {
                uint32_t instr = assem.assemble(editor_buffer[i], i * 4);
                if (instr != (uint32_t)-1) {
                    cpu.store(i * 4, instr);
                } else {
                    // Use official NOP opcode (0x14) shifted left by 26
                    cpu.store(i * 4, 0x14 << 26); 
                }
            }
        } catch (...) {}
        return;
    }

    switch (ch) {
        case KEY_LEFT: if (cursor_x > 0) cursor_x--; break;
        case KEY_RIGHT: if (cursor_x < (int)editor_buffer[cursor_y].size()) cursor_x++; break;
        case KEY_UP: if (cursor_y > 0) { cursor_y--; cursor_x = std::min(cursor_x, (int)editor_buffer[cursor_y].size()); } break;
        case KEY_DOWN: if (cursor_y < (int)editor_buffer.size() - 1) { cursor_y++; cursor_x = std::min(cursor_x, (int)editor_buffer[cursor_y].size()); } break;
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
            std::string remaining = editor_buffer[cursor_y].substr(cursor_x);
            editor_buffer[cursor_y] = editor_buffer[cursor_y].substr(0, cursor_x);
            editor_buffer.insert(editor_buffer.begin() + cursor_y + 1, remaining);
            cursor_y++;
            cursor_x = 0;
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

void TUI::handle_command_mode(int ch) {
    if (ch == 27) { // ESC
        mode = Mode::NORMAL;
        command_buffer = "";
        return;
    }

    if (ch == KEY_BACKSPACE || ch == 127) {
        if (command_buffer.size() > 1) {
            command_buffer.pop_back();
        } else {
            mode = Mode::NORMAL;
            command_buffer = "";
        }
    } else if (ch == '\n' || ch == KEY_ENTER) {
        std::string cmd = command_buffer.substr(1); // skip ':'
        
        if (cmd.starts_with("w ")) {
            std::string filename = cmd.substr(2);
            std::ofstream out(filename);
            for (const auto& line : editor_buffer) {
                out << line << "\n";
            }
            command_buffer = "Written to " + filename;
        } else if (cmd.starts_with("e ")) {
            std::string filename = cmd.substr(2);
            std::ifstream in(filename);
            if (in) {
                editor_buffer.clear();
                std::string line;
                while (std::getline(in, line)) {
                    editor_buffer.push_back(line);
                }
                if (editor_buffer.empty()) editor_buffer.push_back("");
                cursor_y = 0;
                cursor_x = 0;
                command_buffer = "Loaded " + filename;
                
                // Re-assemble immediately
                handle_insert_mode(27); // Trigger the ESC logic to re-assemble
            } else {
                command_buffer = "Error loading " + filename;
            }
        } else if (cmd == "q") {
            running = false;
        } else {
            command_buffer = "Unknown command: " + cmd;
        }
        mode = Mode::NORMAL; // Switch back but draw_status will show the message briefly
    } else if (isprint(ch)) {
        command_buffer += (char)ch;
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
