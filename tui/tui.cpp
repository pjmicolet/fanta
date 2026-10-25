#include "tui.hpp"
#include "disasm.hpp"
#include "instructions.hpp"
#include "terminal.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

using Fanta::Program::kStackTop;

namespace {

// Cycles per 60 Hz UI frame in continuous mode.
constexpr int kBatchCycles = 150000;

auto mode_name(Mode m) -> const char * {
  switch (m) {
  case Mode::NORMAL:
    return "NORMAL";
  case Mode::INSERT:
    return "INSERT";
  case Mode::ZOOM:
    return "ZOOM";
  case Mode::COMMAND:
    return "COMMAND";
  }
  return "?";
}

/// Start of the first word on a line (skips indentation).
auto first_word(const std::string &line) -> std::string {
  auto start = line.find_first_not_of(" \t");
  if (start == std::string::npos)
    return "";
  return line.substr(start);
}

} // namespace

TUI::TUI(CPU &cpu) : cpu(cpu) {
  init_trie();
  last_ips_time = std::chrono::steady_clock::now();
}

void TUI::init_trie() {
  for (auto const &[mnem, meta] : Instructions::Registry::get()) {
    trie.insert(std::string(mnem));
  }
}

TUI::~TUI() {
  if (ncurses_active)
    endwin();
}

void TUI::init_ncurses() {
  initscr();
  ncurses_active = true;
  raw();
  keypad(stdscr, TRUE);
  noecho();
  set_escdelay(25); // Default is 1s, which makes ESC feel stuck
  curs_set(1);      // Show cursor for editor
  timeout(50);      // 50ms delay for continuous mode feel
  if (has_colors()) {
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);    // Registers
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Disassembly / Normal Mode
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Status / Insert Mode
    init_pair(4, COLOR_RED, COLOR_BLACK);     // Halted / errors
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Memory
    init_pair(6, COLOR_WHITE, COLOR_BLACK);   // VRAM preview (default)

    // Initialize 6x6x6 color cube starting at pair 10
    // xterm colors 16-231 are the 6x6x6 cube
    if (COLORS >= 256) {
      for (int r = 0; r < 6; ++r) {
        for (int g = 0; g < 6; ++g) {
          for (int b = 0; b < 6; ++b) {
            int color_idx = 16 + (r * 36) + (g * 6) + b;
            int pair_idx = 10 + (r * 36) + (g * 6) + b;
            init_pair(pair_idx, color_idx,
                      color_idx); // Same fg/bg for solid block
          }
        }
      }
    }
  }
}

void TUI::run() {
  auto frame_start = std::chrono::steady_clock::now();
  const std::chrono::microseconds frame_duration(16666); // ~60 FPS

  while (running) {
    auto now = std::chrono::steady_clock::now();

    if (is_running_continuously)
      run_batch();

    // IPS calculation
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - last_ips_time)
                       .count();
    if (elapsed >= 1000) {
      current_ips = (total_cycles * 1000.0) / elapsed;
      total_cycles = 0;
      last_ips_time = now;
    }

    update();
    handle_input();

    // Sync to 60 FPS
    auto frame_end = std::chrono::steady_clock::now();
    auto sleep_time =
        frame_duration - std::chrono::duration_cast<std::chrono::microseconds>(
                             frame_end - frame_start);
    if (sleep_time.count() > 0) {
      std::this_thread::sleep_for(sleep_time);
    }
    frame_start = std::chrono::steady_clock::now();
  }
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

void TUI::track_changed_registers(
    const std::array<std::uint32_t, 17> &before) {
  last_changed_reg = -1;
  for (int i = 0; i < 17; ++i) {
    if (cpu.registers[i] != before[i]) {
      last_changed_reg = i;
      break;
    }
  }
}

void TUI::step() {
  if (cpu.halted)
    return;
  auto before = cpu.registers;
  cpu.run_cycle();
  total_cycles++;
  track_changed_registers(before);
}

void TUI::run_batch() {
  // Snapshot once per batch rather than per cycle: the highlight only needs
  // to reflect what changed since the last frame.
  auto before = cpu.registers;
  for (int i = 0; i < kBatchCycles; ++i) {
    if (cpu.halted) {
      is_running_continuously = false;
      break;
    }
    cpu.run_cycle();
    total_cycles++;
  }
  track_changed_registers(before);
}

void TUI::assemble_editor() {
  editor.trim_trailing();
  asm_errors = Fanta::Program::assembleInto(cpu, assem, editor.lines());
  if (asm_errors.empty()) {
    message = std::format("Assembled {} lines", editor.lines().size());
  } else {
    const auto &first = asm_errors.front();
    message = std::format("{} line(s) failed to assemble (became NOP). "
                          "First, line {}: {}",
                          asm_errors.size(), first.line, first.text);
  }
}

void TUI::reset() {
  // A fresh CPU resets registers, SP, flags, pending interrupts, VBLANK
  // timing and all of RAM/VRAM in one go.
  cpu = CPU{};
  last_changed_reg = -1;
  is_running_continuously = false;
  if (source == Source::BINARY) {
    Fanta::Program::storeImage(cpu, binary_image);
    message = "Reset";
  } else {
    assemble_editor();
  }
}

void TUI::reset_zoom() {
  mode = Mode::NORMAL;
  zoom_scale = 1.0;
  zoom_cx = 160.0;
  zoom_cy = 120.0;
}

// ---------------------------------------------------------------------------
// Headless dump
// ---------------------------------------------------------------------------

void TUI::dump_state(const std::string &filename) {
  std::ofstream out(filename);
  if (!out)
    return;

  out << "--- TUI DUMP ---" << "\n";
  out << "PC: 0x" << std::hex << cpu.get_pc() << " SP: 0x" << cpu.get_sp()
      << std::dec << " FLAGS: Z:" << (int)cpu.status_reg[0]
      << " N:" << (int)cpu.status_reg[1] << " V:" << (int)cpu.status_reg[2]
      << " C:" << (int)cpu.status_reg[3] << "\n";
  out << "SOURCE: " << (source == Source::ASM ? "ASM" : "BINARY (read-only)")
      << "\n";
  if (!message.empty())
    out << "MESSAGE: " << message << "\n";
  out << "----------------" << "\n";

  out << "[REGISTERS]" << "\n";
  for (int i = 0; i < 17; ++i) {
    std::string label = Fanta::getRegisterName(static_cast<uint8_t>(i));
    out << std::left << std::setw(3) << std::setfill(' ') << label << ": 0x"
        << std::right << std::hex << std::uppercase << std::setw(8)
        << std::setfill('0') << cpu.registers[i] << std::dec
        << std::nouppercase << "  ";
    if (i % 4 == 3 || i == 16)
      out << "\n";
  }

  out << "\n" << "[DISASSEMBLY]" << "\n";
  int64_t pc = cpu.get_pc();
  for (int i = -5; i <= 5; ++i) {
    int64_t addr = pc + (i * 4);
    if (addr < 0 || addr > static_cast<int64_t>(cpu.ram.getSize()) - 4)
      continue; // PC underflow / past end of RAM
    out << (i == 0 ? "> " : "  ") << "0x" << std::hex << addr << std::dec
        << ": " << disassemble(static_cast<uint32_t>(addr)) << "\n";
  }

  out << "\n" << "[EDITOR]" << "\n";
  const auto &lines = editor.lines();
  for (size_t i = 0; i < lines.size(); ++i) {
    bool bad = std::ranges::any_of(
        asm_errors, [i](const auto &e) { return e.line == i; });
    out << std::setw(2) << std::setfill(' ') << i << (bad ? "!" : "|") << " "
        << lines[i] << (i == (size_t)editor.y() ? " <--" : "") << "\n";
  }

  out << "\n" << "[SERIAL]" << "\n";
  for (const auto &line : Fanta::Terminal::layout(serial_output(), 80))
    out << line << "\n";

  if (!asm_errors.empty()) {
    out << "\n" << "[ASM ERRORS]" << "\n";
    for (const auto &e : asm_errors)
      out << "line " << e.line << ": " << e.text << "\n";
  }

  out << "--- END DUMP ---" << "\n";
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

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
  draw_stack();
  draw_editor();
  if (show_serial)
    draw_serial();
  else
    draw_vram_preview();

  // Leave the terminal cursor on the editor cursor.
  int editor_y = term_h * 0.6;
  move(editor_y + (editor.y() - editor_scroll), 6 + editor.x());

  refresh();
}

void TUI::draw_registers() {
  attron(COLOR_PAIR(1));
  mvprintw(1, 2, "--- REGISTERS ---");
  attroff(COLOR_PAIR(1));

  for (int i = 0; i < 17; ++i) {
    int col;
    int row;
    if (i < 8) {
      col = 2;
      row = 2 + i;
    } else if (i < 16) {
      col = 20;
      row = 2 + (i - 8);
    } else {
      col = 2; // Put SP below R7
      row = 10;
    }

    if (row >= term_h)
      continue;

    std::string label = Fanta::getRegisterName(static_cast<uint8_t>(i));
    int attrs =
        (i == last_changed_reg) ? (COLOR_PAIR(3) | A_BOLD) : COLOR_PAIR(1);
    attron(attrs);
    mvprintw(row, col, "%-3s: 0x%08X", label.c_str(), cpu.registers[i]);
    attroff(attrs);
  }
}

void TUI::draw_status() {
  attron(COLOR_PAIR(3));
  int col = term_w * 0.25;
  if (col < 20)
    col = 20;
  mvprintw(1, col, "--- STATUS ---");
  mvprintw(2, col, "PC: 0x%04X | SP: 0x%04X", cpu.get_pc(), cpu.get_sp());
  mvprintw(3, col, "FLAGS: Z:%d N:%d V:%d C:%d", cpu.status_reg[0],
           cpu.status_reg[1], (cpu.status_reg[2] ? 1 : 0),
           (cpu.status_reg[3] ? 1 : 0));
  mvprintw(4, col, "IPS: %.0f | SERIAL: %zu bytes (o)", current_ips,
           serial_output().size());
  mvprintw(5, col, "MODE: %s %s", mode_name(mode),
           (is_running_continuously ? "(CONT)" : ""));
  mvprintw(6, col, "KEYS: %s",
           (mode == Mode::NORMAL ? "s:step c:cont r:reset i:edit q:quit"
                                 : "ESC:exit"));
  attroff(COLOR_PAIR(3));

  // Bottom line: the command being typed, else the last message.
  const std::string &bottom =
      (mode == Mode::COMMAND) ? command_buffer : message;
  if (!bottom.empty()) {
    int attrs = (mode != Mode::COMMAND && !asm_errors.empty())
                    ? (COLOR_PAIR(4) | A_REVERSE)
                    : A_REVERSE;
    attron(attrs);
    mvprintw(term_h - 1, 0, "%s", bottom.c_str());
    attroff(attrs);
  }

  if (cpu.halted) {
    attron(COLOR_PAIR(4) | A_BOLD);
    mvprintw(7, col, "!!! HALTED !!!");
    attroff(COLOR_PAIR(4) | A_BOLD);
  }
}

void TUI::draw_disassembly() {
  attron(COLOR_PAIR(2));
  int col = term_w * 0.60;
  if (col < 48)
    col = 48;
  mvprintw(1, col, "--- DISASSEMBLY ---");

  // Highlight what is ABOUT TO RUN (the current PC)
  uint32_t current_pc = cpu.get_pc();

  int max_lines = (term_h * 0.5) - 2;
  if (max_lines < 5)
    max_lines = 5;

  for (int i = 0; i < max_lines; ++i) {
    int64_t addr = int64_t{current_pc} - (max_lines / 2 * 4) + (i * 4);
    if (addr < 0 || addr > static_cast<int64_t>(cpu.ram.getSize()) - 4)
      continue;
    if (2 + i >= term_h * 0.6)
      break;

    std::string line = disassemble((uint32_t)addr);
    if ((uint32_t)addr == current_pc) {
      attron(A_BOLD | A_REVERSE);
      mvprintw(2 + i, col, "> 0x%04X: %s", (uint32_t)addr, line.c_str());
      attroff(A_BOLD | A_REVERSE);
    } else {
      mvprintw(2 + i, col, "0x%04X: %s", (uint32_t)addr, line.c_str());
    }
  }
  attroff(COLOR_PAIR(2));
}

void TUI::draw_memory() {
  attron(COLOR_PAIR(5));
  int start_y = term_h * 0.4;
  if (start_y < 10)
    start_y = 10;

  mvprintw(start_y, 2, "--- MEM (0x%04X) ---", mem_offset);
  for (int i = 0; i < 4; ++i) {
    uint32_t row_addr = mem_offset + (i * 8);
    if (row_addr + 8 > cpu.ram.getSize() || start_y + 1 + i >= term_h * 0.6)
      break;
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(4) << std::setfill('0') << row_addr
       << ": ";
    uint8_t *ptr = cpu.ram.from(row_addr);
    for (int j = 0; j < 8; ++j) {
      ss << std::hex << std::setw(2) << std::setfill('0') << (int)ptr[j] << " ";
    }
    mvprintw(start_y + 1 + i, 2, "%s", ss.str().c_str());
  }
  attroff(COLOR_PAIR(5));
}

void TUI::draw_stack() {
  attron(COLOR_PAIR(3));
  int start_y = term_h * 0.4;
  if (start_y < 10)
    start_y = 10;

  int col = 34;
  mvprintw(start_y, col, "--- STACK ---");

  // PUSH writes at SP then decrements, so SP is the next free slot and the
  // top value lives at SP + 4.
  uint32_t sp = cpu.registers[16];
  uint32_t fp = cpu.registers[15];
  if (sp >= kStackTop) {
    mvprintw(start_y + 1, col, "(empty)");
    attroff(COLOR_PAIR(3));
    return;
  }

  int row_offset = 0;
  for (uint32_t addr = sp + 4; addr <= kStackTop && row_offset < 4;
       addr += 4) {
    if (start_y + 1 + row_offset >= term_h * 0.6)
      break;

    uint32_t val = cpu.ram.read32(addr);
    const char *fp_marker = (addr == fp) ? " <- FP" : "";
    const char *tag = (row_offset == 0) ? "TOP ->" : "      ";
    mvprintw(start_y + 1 + row_offset, col, "%s 0x%06X: 0x%08X%s", tag, addr,
             val, fp_marker);
    row_offset++;
  }
  attroff(COLOR_PAIR(3));
}

void TUI::draw_editor() {
  int win_y = term_h * 0.6;
  int win_h = term_h - win_y - 1;
  if (win_h < 3)
    win_h = 3;

  attron(A_BOLD);
  mvprintw(win_y - 1, 2, "%s",
           source == Source::ASM ? "--- EDITOR (Mnemonic Entry) ---"
                                 : "--- EDITOR (binary, read-only) ---");
  attroff(A_BOLD);

  // Calculate scrolling to keep cursor in view
  int visible_lines = win_h - 1;
  if (editor.y() < editor_scroll) {
    editor_scroll = editor.y();
  } else if (editor.y() >= editor_scroll + visible_lines) {
    editor_scroll = editor.y() - visible_lines + 1;
  }

  const auto &lines = editor.lines();
  for (int i = 0; i < visible_lines; ++i) {
    int line_idx = editor_scroll + i;
    if (line_idx >= (int)lines.size())
      break;

    const std::string &line = lines[line_idx];
    bool bad = std::ranges::any_of(asm_errors, [line_idx](const auto &e) {
      return e.line == (size_t)line_idx;
    });
    int attrs = bad ? COLOR_PAIR(4)
                    : (line_idx == editor.y() ? COLOR_PAIR(2) : A_NORMAL);
    attron(attrs);
    mvprintw(win_y + i, 2, "%2d%c %s", line_idx, bad ? '!' : '|',
             line.c_str());
    attroff(attrs);

    // Ghost-text mnemonic suggestion while typing the first word.
    if (line_idx == editor.y() && mode == Mode::INSERT) {
      std::string word = first_word(line);
      if (!word.empty() && word.find_first_of(" \t") == std::string::npos) {
        std::string suggestion = trie.getSuggestion(word);
        if (suggestion.size() > word.size()) {
          attron(A_DIM);
          mvprintw(win_y + i, 6 + line.size(), "%s",
                   suggestion.substr(word.size()).c_str());
          attroff(A_DIM);
        }
      }
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
    start_col = term_w * 0.60;
    start_y = term_h * 0.6;
    preview_w = term_w - start_col - 4;
    preview_h = term_h - start_y - 1;
    mvprintw(start_y - 1, start_col, "--- VRAM MONITOR ---");
  }

  if (preview_w <= 0 || preview_h <= 0)
    return;

  uint32_t *vram = (uint32_t *)cpu.get_vram();
  bool use_color = (COLORS >= 256);

  // Centered magnifying glass calculations
  double visible_w = 320.0 / zoom_scale;
  double visible_h = 240.0 / zoom_scale;
  double zoom_start_x = zoom_cx - (visible_w / 2.0);
  double zoom_start_y = zoom_cy - (visible_h / 2.0);

  // Clamp viewport to VRAM boundaries
  if (zoom_start_x < 0.0)
    zoom_start_x = 0.0;
  if (zoom_start_x + visible_w > 320.0)
    zoom_start_x = 320.0 - visible_w;
  if (zoom_start_y < 0.0)
    zoom_start_y = 0.0;
  if (zoom_start_y + visible_h > 240.0)
    zoom_start_y = 240.0 - visible_h;

  for (int y = 0; y < preview_h; ++y) {
    int vy = (int)(zoom_start_y + (y * visible_h) / preview_h);
    if (vy < 0)
      vy = 0;
    if (vy >= 240)
      break;
    uint32_t *row = vram + (vy * 320);

    move(start_y + y, start_col);
    for (int x = 0; x < preview_w; ++x) {
      int vx = (int)(zoom_start_x + (x * visible_w) / preview_w);
      if (vx < 0)
        vx = 0;
      if (vx >= 320)
        break;

      uint32_t pixel = row[vx];
      uint8_t r = (pixel >> 16) & 0xFF;
      uint8_t g = (pixel >> 8) & 0xFF;
      uint8_t b = pixel & 0xFF;

      if (use_color) {
        int r5 = (r * 5) / 255;
        int g5 = (g * 5) / 255;
        int b5 = (b * 5) / 255;
        int pair_idx = 10 + (r5 * 36) + (g5 * 6) + b5;

        attron(COLOR_PAIR(pair_idx));
        addch(' ');
        attroff(COLOR_PAIR(pair_idx));
      } else {
        const char *ramp = " .:-=+*#%@";
        uint8_t lum = (r + g + b) / 3;
        addch(ramp[(lum * 9) / 255]);
      }
    }
  }

  // Draw vivid '+' crosshair cursor at the zoom center coordinate in ZOOM mode
  if (mode == Mode::ZOOM) {
    int cur_sx =
        start_col + (int)((zoom_cx - zoom_start_x) * preview_w / visible_w);
    int cur_sy =
        start_y + (int)((zoom_cy - zoom_start_y) * preview_h / visible_h);
    if (cur_sx >= 0 && cur_sx < term_w && cur_sy >= 0 && cur_sy < term_h) {
      attron(COLOR_PAIR(3) | A_BOLD | A_REVERSE);
      mvaddch(cur_sy, cur_sx, '+');
      attroff(COLOR_PAIR(3) | A_BOLD | A_REVERSE);
    }
  }
}

auto TUI::serial_output() -> const std::string & {
  return cpu.bus.get<BasicSerial<>>().output;
}

void TUI::draw_serial() {
  int start_col = term_w * 0.60;
  int start_y = term_h * 0.6;
  int pane_w = term_w - start_col - 4;
  int pane_h = term_h - start_y - 1;
  mvprintw(start_y - 1, start_col, "--- SERIAL (%zu bytes) ---",
           serial_output().size());
  if (pane_w <= 0 || pane_h <= 0)
    return;

  // Like a terminal: show the most recent lines, oldest at the top.
  auto lines = Fanta::Terminal::layout(serial_output(), pane_w);
  size_t first = lines.size() > (size_t)pane_h ? lines.size() - pane_h : 0;
  for (size_t i = first; i < lines.size(); ++i)
    mvprintw(start_y + (int)(i - first), start_col, "%s", lines[i].c_str());
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void TUI::handle_input() {
  int ch = getch();
  if (ch == ERR)
    return;

  if (ch == 27) { // ESC always stops a continuous run
    is_running_continuously = false;
  }

  switch (mode) {
  case Mode::NORMAL:
    handle_normal_mode(ch);
    break;
  case Mode::INSERT:
    handle_insert_mode(ch);
    break;
  case Mode::ZOOM:
    handle_zoom_mode(ch);
    break;
  case Mode::COMMAND:
    handle_command_mode(ch);
    break;
  }
}

void TUI::handle_zoom_mode(int ch) {
  // In ZOOM mode, we still want to step, reset, and continue
  switch (ch) {
  case 27: // ESC
  case 'z':
    reset_zoom();
    break;
  case 's':
    step();
    break;
  case 'c':
    if (!cpu.halted)
      is_running_continuously = !is_running_continuously;
    break;
  case 'r':
    reset();
    break;
  case '+':
  case '=':
    zoom_scale = std::min(zoom_scale + 0.25, 10.0);
    break;
  case '-':
    zoom_scale = std::max(zoom_scale - 0.25, 1.0);
    break;
  case 'h':
  case KEY_LEFT:
    zoom_cx = std::max(zoom_cx - 4.0, 0.0);
    break;
  case 'l':
  case KEY_RIGHT:
    zoom_cx = std::min(zoom_cx + 4.0, 320.0);
    break;
  case 'i':
  case 'k':
  case KEY_UP:
    zoom_cy = std::max(zoom_cy - 4.0, 0.0);
    break;
  case 'j':
  case KEY_DOWN:
    zoom_cy = std::min(zoom_cy + 4.0, 240.0);
    break;
  }
}

void TUI::handle_normal_mode(int ch) {
  switch (ch) {
  case 'q':
    running = false;
    break;
  case ':':
    mode = Mode::COMMAND;
    command_buffer = ":";
    is_running_continuously = false;
    break;
  case 'z':
    mode = Mode::ZOOM;
    is_running_continuously = false;
    break;
  case 'o':
    show_serial = !show_serial;
    break;
  case '[':
    if (mem_offset >= 16)
      mem_offset -= 16;
    break;
  case ']':
    if (mem_offset + 16 + 32 <= cpu.ram.getSize()) // keep the 32-byte pane in RAM
      mem_offset += 16;
    break;
  case 'r':
    reset();
    break;
  case 'i':
    if (editor.read_only()) {
      message = "Editor is read-only for binaries; :e a .asm file to edit";
      break;
    }
    mode = Mode::INSERT;
    curs_set(1);
    is_running_continuously = false;
    break;
  case 's':
    step();
    break;
  case 'c':
    if (!cpu.halted)
      is_running_continuously = !is_running_continuously;
    break;
  case KEY_UP:
    editor.up();
    break;
  case KEY_DOWN:
    editor.down();
    break;
  case KEY_LEFT:
    editor.left();
    break;
  case KEY_RIGHT:
    editor.right();
    break;
  case 'x': // Delete character
    editor.erase_at_cursor();
    break;
  }
}

void TUI::handle_insert_mode(int ch) {
  switch (ch) {
  case 27: // ESC: leave insert mode and live-patch memory with the buffer
    mode = Mode::NORMAL;
    curs_set(0);
    assemble_editor();
    break;
  case KEY_LEFT:
    editor.left();
    break;
  case KEY_RIGHT:
    editor.right();
    break;
  case KEY_UP:
    editor.up();
    break;
  case KEY_DOWN:
    editor.down();
    break;
  case '\t': { // TAB completion of the mnemonic
    std::string word = first_word(editor.line());
    if (!word.empty() && word.find_first_of(" \t") == std::string::npos) {
      std::string suggestion = trie.getSuggestion(word);
      if (!suggestion.empty())
        editor.replace_first_word(suggestion);
    }
    break;
  }
  case KEY_BACKSPACE:
  case 127:
    editor.backspace();
    break;
  case KEY_ENTER:
  case '\n':
    editor.newline();
    break;
  default:
    if (isprint(ch))
      editor.insert(static_cast<char>(ch));
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
    command_buffer = "";
    mode = Mode::NORMAL;

    if (cmd.starts_with("w ")) {
      std::string filename = cmd.substr(2);
      std::ofstream out(filename);
      if (!out) {
        message = "Error writing " + filename;
        return;
      }
      for (const auto &line : editor.lines()) {
        out << line << "\n";
      }
      message = "Written to " + filename;
    } else if (cmd.starts_with("e ")) {
      std::string filename = cmd.substr(2);
      if (!load_file(filename))
        message = "Error loading " + filename;
    } else if (cmd == "q") {
      running = false;
    } else {
      message = "Unknown command: " + cmd;
    }
  } else if (isprint(ch)) {
    command_buffer += (char)ch;
  }
}

// ---------------------------------------------------------------------------
// Program loading / disassembly
// ---------------------------------------------------------------------------

std::string TUI::disassemble(uint32_t addr) {
  if (addr > cpu.ram.getSize() - 4)
    return "OUT OF BOUNDS";
  auto labels = [this](uint32_t a) -> std::optional<std::string> {
    auto label = assem.get_label_for(static_cast<int>(a));
    if (label.empty())
      return std::nullopt;
    return label;
  };
  std::string text = Fanta::disassemble(cpu.ram.read32(addr), addr, labels);
  if (auto here = labels(addr))
    return "[" + *here + "] " + text;
  return text;
}

bool TUI::load_file(const std::string &filename) {
  editor_scroll = 0;
  if (filename.ends_with(".bin")) {
    auto image = Fanta::Program::readBinary(filename);
    if (!image)
      return false;

    binary_image = std::move(*image);
    source = Source::BINARY;
    assem = Assembler(); // Binaries carry no labels
    asm_errors.clear();
    reset();

    // The editor shows a disassembly view. It is read-only: the text can't
    // be assembled back into the same program (branch offsets, labels), so
    // the image stays the source of truth.
    std::vector<std::string> view;
    for (size_t i = 0; i < binary_image.size(); ++i) {
      view.push_back(Fanta::disassemble(binary_image[i],
                                        static_cast<uint32_t>(i * 4)));
    }
    editor.set_lines(std::move(view), /*read_only=*/true);
    message = std::format("Loaded {} ({} words, read-only)", filename,
                          binary_image.size());
    return true;
  }

  auto lines = Fanta::Program::readLines(filename);
  if (!lines)
    return false;

  source = Source::ASM;
  binary_image.clear();
  editor.set_lines(std::move(*lines));
  reset(); // fresh CPU, then assemble the new buffer
  if (asm_errors.empty())
    message = "Loaded " + filename;
  return true;
}
