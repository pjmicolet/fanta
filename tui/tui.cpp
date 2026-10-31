#include "tui.hpp"
#include "build.hpp"
#include "canvas.hpp"
#include "disasm.hpp"
#include "instructions.hpp"
#include "terminal.hpp"
#include <algorithm>
#include <filesystem>
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
  configure_ncurses();
}

void TUI::configure_ncurses() {
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
  changed_regs.reset();
  for (int i = 0; i < 17; ++i)
    changed_regs.set(i, cpu.registers[i] != before[i]);
}

void TUI::follow_pc() {
  if (source != Source::ASM || mode == Mode::INSERT)
    return;
  auto line = static_cast<int>(display_pc() / 4);
  if (line < static_cast<int>(editor.lines().size()))
    editor.goto_line(line);
}

void TUI::step() {
  if (cpu.halted)
    return;
  auto before = cpu.registers;
  cpu.run_cycle();
  total_cycles++;
  track_changed_registers(before);
  follow_pc();
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
  follow_pc();
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

bool TUI::compile_editor() {
  editor.trim_trailing();
  ++build_count;
  auto result = Fanta::Build::compile(editor.text());
  if (!result) {
    build_failed = true;
    message = "Compile error: " + result.error();
    return false;
  }
  build_failed = false;
  binary_image = std::move(*result);
  message = std::format("Compiled {} instructions", binary_image.size());
  return true;
}

void TUI::reset() {
  // Compile before touching the CPU, so a failed build keeps the last good
  // program loaded and debuggable.
  if (source == Source::FANTA && !compile_editor())
    return;

  // A fresh CPU resets registers, SP, flags, pending interrupts, VBLANK
  // timing and all of RAM/VRAM in one go.
  cpu = CPU{};
  changed_regs.reset();
  is_running_continuously = false;
  switch (source) {
  case Source::BINARY:
    Fanta::Program::storeImage(cpu, binary_image);
    message = "Reset";
    break;
  case Source::FANTA:
    Fanta::Program::storeImage(cpu, binary_image); // message set by compile
    break;
  case Source::ASM:
    assemble_editor();
    break;
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
  out << "SOURCE: "
      << (source == Source::ASM      ? "ASM"
          : source == Source::FANTA ? "FANTA"
                                    : "BINARY (read-only)")
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
// Drawing (docs/tui_spec.md §3). Every pane draws through a Canvas clipped to
// the rectangle Layout::compute gives it, so panes can't overlap.
// ---------------------------------------------------------------------------

namespace {

constexpr attr_t kChanged = A_BOLD;
constexpr short kPairRegs = 1;
constexpr short kPairCode = 2;
constexpr short kPairStatus = 3;
constexpr short kPairError = 4;
constexpr short kPairMemory = 5;

auto hex8(uint32_t v) -> std::string { return std::format("{:08X}", v); }

/// File name without directories, for pane titles (the status line has the
/// full path).
auto base_name(const std::string &p) -> std::string {
  return p.empty() ? "(unsaved)" : std::filesystem::path(p).filename().string();
}

/// The longest hint that fits: full text, else the short form.
auto fit(std::string_view full, std::string_view brief, int width)
    -> std::string_view {
  return static_cast<int>(full.size()) <= width ? full : brief;
}
auto addr(uint32_t v) -> std::string { return std::format("0x{:06X}", v); }

auto tab_name(OutputTab t) -> std::string_view {
  switch (t) {
  case OutputTab::SERIAL:
    return "Serial";
  case OutputTab::VRAM:
    return "VRAM";
  case OutputTab::MEMORY:
    return "Memory";
  case OutputTab::LOG:
    return "Build log";
  }
  return "?";
}

/// "[Serial]  VRAM  Memory  Build log" with the current tab bracketed.
auto tab_title(OutputTab current) -> std::string {
  std::string t;
  for (auto tab : {OutputTab::SERIAL, OutputTab::VRAM, OutputTab::MEMORY,
                   OutputTab::LOG}) {
    if (!t.empty())
      t += "  ";
    t += tab == current ? "[" + std::string{tab_name(tab)} + "]"
                        : std::string{tab_name(tab)};
  }
  return t;
}

} // namespace

auto TUI::layout_kind() const -> Fanta::Layout::Kind {
  switch (source) {
  case Source::FANTA:
    return Fanta::Layout::Kind::SOURCE;
  case Source::BINARY:
    return Fanta::Layout::Kind::BINARY;
  case Source::ASM:
    break;
  }
  return Fanta::Layout::Kind::ASM;
}

auto TUI::display_pc() const -> uint32_t {
  // After HALT, PC has already moved past it; point at the HALT itself.
  uint32_t pc = cpu.get_pc();
  return (cpu.halted && pc >= 4) ? pc - 4 : pc;
}

void TUI::update() {
  getmaxyx(stdscr, term_h, term_w);
  erase();
  cursor_at.reset();

  if (!message.empty() && message != last_logged) {
    build_log.push_back(message);
    last_logged = message;
  }

  if (mode == Mode::ZOOM) {
    draw_vram(Fanta::Canvas{{0, 0, term_w, term_h}}, /*zoomed=*/true);
    curs_set(0);
    refresh();
    return;
  }

  auto l = Fanta::Layout::compute(term_w, term_h, layout_kind());
  draw_status_line(Fanta::Canvas{l.status});

  if (output_overlay) {
    // Small terminal: the output panel takes over the work area until ESC.
    int top = l.status.bottom();
    draw_output(Fanta::Canvas{{0, top, term_w, l.hint.y - top}}, output_tab);
  } else {
    switch (source) {
    case Source::FANTA:
      draw_source(Fanta::Canvas{l.code});
      if (l.derived)
        draw_disasm_view(Fanta::Canvas{*l.derived},
                         std::format("ASM  build #{}  (derived, read-only)",
                                     build_count));
      break;
    case Source::ASM:
      draw_asm_editor(Fanta::Canvas{l.code});
      break;
    case Source::BINARY:
      draw_disasm_view(
          Fanta::Canvas{l.code},
          std::format("BINARY  {}  (read-only image)", base_name(path)));
      break;
    }
    if (l.registers)
      draw_registers(Fanta::Canvas{*l.registers}, l.register_form);
    if (l.stack)
      draw_stack(Fanta::Canvas{*l.stack});
    if (l.output)
      draw_output(Fanta::Canvas{*l.output}, output_tab);
    if (l.output_secondary)
      draw_output(Fanta::Canvas{*l.output_secondary},
                  output_tab == OutputTab::VRAM ? OutputTab::SERIAL
                                                : OutputTab::VRAM);
  }

  draw_hint_bar(Fanta::Canvas{l.hint});

  if (mode == Mode::COMMAND) {
    curs_set(1);
    move(l.hint.y, static_cast<int>(command_buffer.size()));
  } else if (cursor_at) {
    curs_set(1);
    move(cursor_at->first, cursor_at->second);
  } else {
    curs_set(0);
  }
  refresh();
}

void TUI::draw_status_line(const Fanta::Canvas &c) {
  std::string kind;
  std::string build;
  switch (source) {
  case Source::FANTA:
    kind = "SOURCE";
    build = build_count == 0 ? "not built"
            : build_failed   ? std::format("build #{} FAILED", build_count)
                             : std::format("build #{} OK {}w", build_count,
                                           binary_image.size());
    break;
  case Source::ASM:
    kind = "ASM";
    build = asm_errors.empty()
                ? std::format("asm OK {} lines", editor.lines().size())
                : std::format("asm {} error(s)", asm_errors.size());
    break;
  case Source::BINARY:
    kind = "BINARY";
    build = std::format("image {}w", binary_image.size());
    break;
  }

  std::string state =
      cpu.halted ? "HALTED at " + addr(display_pc())
                 : std::format("{} pc {}",
                               is_running_continuously ? "RUNNING" : "PAUSED",
                               addr(display_pc()));
  auto line_for = [&](const std::string &file) {
    return std::format(" {} | {} {} | {} | {} ", mode_name(mode), kind, file,
                       build, state);
  };
  std::string left = line_for(path.empty() ? "(no file)" : path);
  if (static_cast<int>(left.size()) > c.width() - 20)
    left = line_for(base_name(path));
  if (term_w < Fanta::Layout::kMinCols || term_h < Fanta::Layout::kMinRows)
    left += std::format("| terminal {}x{} < 80x24 ", term_w, term_h);

  c.put_line(0, left, A_REVERSE);
  if (!message.empty()) {
    int col = static_cast<int>(left.size()) + 1;
    attr_t a = A_REVERSE;
    if (build_failed || !asm_errors.empty())
      a |= COLOR_PAIR(kPairError) | A_BOLD;
    c.put(0, col, message, a);
  }
}

void TUI::draw_hint_bar(const Fanta::Canvas &c) {
  if (mode == Mode::COMMAND) {
    c.put_line(0, command_buffer);
    return;
  }
  std::string_view hint;
  int w = c.width();
  switch (mode) {
  case Mode::INSERT:
    hint = source == Source::ASM ? " ESC normal (re-assembles)  TAB complete "
                                 : " ESC normal  TAB indent ";
    break;
  case Mode::ZOOM:
    hint = " +/- zoom  hjkl/arrows pan  s step  c run  r reset  ESC back ";
    break;
  default:
    hint = output_overlay
               ? " o next tab  ESC close output  s step  c run  r reset "
               : fit(" s step  c run/pause  r reset  i edit  o output tab  "
                     "[ ] memory  z zoom VRAM  : cmd  q quit ",
                     " s step  c run  r reset  i edit  o output  z zoom  "
                     ": cmd  q quit ",
                     w);
    break;
  }
  c.put_line(0, hint, A_DIM);
}

void TUI::draw_code_lines(const Fanta::Canvas &c, bool show_addresses,
                          bool editable) {
  const auto &lines = editor.lines();
  int rows = c.height();
  if (editor.y() < editor_scroll)
    editor_scroll = editor.y();
  else if (editor.y() >= editor_scroll + rows)
    editor_scroll = editor.y() - rows + 1;

  uint32_t pc = display_pc();
  for (int row = 0; row < rows; ++row) {
    int idx = editor_scroll + row;
    if (idx >= static_cast<int>(lines.size()))
      break;
    auto uidx = static_cast<size_t>(idx);
    bool bad = std::ranges::any_of(
        asm_errors, [uidx](const auto &e) { return e.line == uidx; });
    bool at_pc = show_addresses && static_cast<uint32_t>(idx * 4) == pc;

    std::string gutter =
        show_addresses ? std::format("{}{} ", at_pc ? '>' : ' ',
                                     addr(static_cast<uint32_t>(idx * 4)))
                       : std::format("{:4} ", idx + 1);
    gutter += bad ? '!' : ' ';
    gutter += ' ';
    attr_t a = bad ? COLOR_PAIR(kPairError)
               : at_pc ? (A_REVERSE | A_BOLD)
                       : A_NORMAL;
    c.put_line(row, gutter + lines[uidx], a);

    if (idx == editor.y()) {
      int col = static_cast<int>(gutter.size()) + editor.x();
      if (editable)
        cursor_at = {c.rect().y + row, c.rect().x + std::min(col, c.width() - 1)};
      // Ghost-text mnemonic suggestion while typing the first word.
      if (mode == Mode::INSERT && source == Source::ASM) {
        std::string word = first_word(lines[uidx]);
        if (!word.empty() && word.find_first_of(" \t") == std::string::npos) {
          std::string suggestion = trie.getSuggestion(word);
          if (suggestion.size() > word.size())
            c.put(row, static_cast<int>(gutter.size() + lines[uidx].size()),
                  suggestion.substr(word.size()), A_DIM);
        }
      }
    }
  }
}

void TUI::draw_source(const Fanta::Canvas &c) {
  auto inner = c.frame(
      std::format("SOURCE  {}  (source of truth)", base_name(path)));
  draw_code_lines(inner, /*show_addresses=*/false, /*editable=*/true);
}

void TUI::draw_asm_editor(const Fanta::Canvas &c) {
  auto inner =
      c.frame(std::format("ASM  {}  (source of truth)", base_name(path)));
  draw_code_lines(inner, /*show_addresses=*/true, /*editable=*/true);
}

void TUI::draw_disasm_view(const Fanta::Canvas &c, std::string_view title) {
  auto inner = c.frame(title, COLOR_PAIR(kPairCode));
  uint32_t pc = display_pc();
  int rows = inner.height();
  // Keep PC a third of the way down, but never start before address 0
  // (no blank rows above the program).
  int64_t start = int64_t{pc} - (rows / 3) * 4;
  start = std::max<int64_t>(start, 0);
  auto ram = static_cast<int64_t>(cpu.ram.getSize());
  for (int row = 0; row < rows; ++row) {
    int64_t a = start + int64_t{row} * 4;
    if (a > ram - 4)
      break;
    auto ua = static_cast<uint32_t>(a);
    bool at_pc = ua == pc;
    std::string line = std::format("{}{}  {}", at_pc ? '>' : ' ', addr(ua),
                                   disassemble(ua));
    inner.put_line(row, line,
                   at_pc ? (A_REVERSE | A_BOLD) : COLOR_PAIR(kPairCode));
  }
}

void TUI::draw_registers(const Fanta::Canvas &c,
                         Fanta::Layout::RegisterForm form) {
  using Fanta::Layout::RegisterForm;
  auto name = [](int i) -> std::string {
    if (i == 15)
      return "FP";
    if (i == 16)
      return "SP";
    return "R" + std::to_string(i);
  };
  // "R1 *0000000A": a '*' and bold mark registers the last step changed.
  auto cell = [&](int i) {
    return std::format("{:<3}{}{}", name(i), changed_regs.test(i) ? '*' : ' ',
                       hex8(cpu.registers[i]));
  };
  auto flags = [&] {
    auto f = [&](int i, char ch) {
      return cpu.status_reg[i] ? ch : static_cast<char>(ch - 'A' + 'a');
    };
    return std::format("FLAGS {} {} {} {}", f(0, 'Z'), f(1, 'N'), f(2, 'V'),
                       f(3, 'C'));
  };
  auto pc_cell = std::format("PC  {}", hex8(display_pc()));
  // Draw cells left to right, bolding the changed ones.
  auto row_of = [&](const Fanta::Canvas &cv, int row,
                    std::initializer_list<int> regs, const std::string &tail,
                    int cell_w) {
    int col = 0;
    for (int i : regs) {
      cv.put(row, col, cell(i),
             changed_regs.test(i) ? (kChanged | COLOR_PAIR(kPairStatus))
                                  : COLOR_PAIR(kPairRegs));
      col += cell_w;
    }
    if (!tail.empty())
      cv.put(row, col, tail, COLOR_PAIR(kPairRegs));
  };

  switch (form) {
  case RegisterForm::FOUR_COLUMNS: {
    auto in = c.frame("REGISTERS", COLOR_PAIR(kPairRegs));
    for (int r = 0; r < 4; ++r)
      row_of(in, r, {r, r + 4, r + 8, r + 12}, "", 15);
    row_of(in, 4, {16}, pc_cell + "   " + flags(), 15);
    break;
  }
  case RegisterForm::TWO_COLUMNS: {
    auto in = c.frame("REGISTERS", COLOR_PAIR(kPairRegs));
    for (int r = 0; r < 8; ++r)
      row_of(in, r, {r, r + 8}, "", 15);
    row_of(in, 8, {16}, pc_cell, 15);
    in.put(9, 0, flags(), COLOR_PAIR(kPairRegs));
    break;
  }
  case RegisterForm::COMPACT:
    row_of(c, 0, {0, 1, 2, 3, 4, 5}, "", 13);
    row_of(c, 1, {6, 7, 8, 9, 10, 11}, flags(), 13);
    row_of(c, 2, {12, 13, 14, 15, 16}, pc_cell, 13);
    break;
  }
}

void TUI::draw_stack(const Fanta::Canvas &c) {
  uint32_t sp = cpu.registers[16];
  uint32_t fp = cpu.registers[15];
  auto inner = c.frame(std::format("STACK  SP {}", addr(sp)),
                       COLOR_PAIR(kPairStatus));
  // PUSH writes at SP then decrements, so the top value lives at SP + 4.
  if (sp >= kStackTop) {
    inner.put(0, 1, "(empty)");
    return;
  }
  int row = 0;
  for (uint32_t a = sp + 4; a <= kStackTop && row < inner.height(); a += 4) {
    std::string line = std::format(" {:06X}  {}", a, hex8(cpu.ram.read32(a)));
    if (row == 0)
      line += "  <- TOP";
    if (a == fp)
      line += "  <- FP";
    inner.put(row++, 0, line);
  }
}

void TUI::draw_output(const Fanta::Canvas &c, OutputTab tab) {
  auto inner = c.frame(tab_title(tab));
  switch (tab) {
  case OutputTab::SERIAL:
    draw_serial(inner);
    break;
  case OutputTab::VRAM:
    draw_vram(inner, /*zoomed=*/false);
    break;
  case OutputTab::MEMORY:
    draw_memory(inner);
    break;
  case OutputTab::LOG: {
    int rows = inner.height();
    size_t first = build_log.size() > static_cast<size_t>(rows)
                       ? build_log.size() - rows
                       : 0;
    for (size_t i = first; i < build_log.size(); ++i)
      inner.put(static_cast<int>(i - first), 0, build_log[i]);
    break;
  }
  }
}

auto TUI::serial_output() -> const std::string & {
  return cpu.bus.get<BasicSerial<>>().output;
}

void TUI::draw_serial(const Fanta::Canvas &c) {
  if (serial_output().empty()) {
    c.put(0, 0, "(no serial output)", A_DIM);
    return;
  }
  // Like a terminal: the most recent lines, oldest at the top.
  auto lines = Fanta::Terminal::layout(serial_output(), c.width());
  size_t first = lines.size() > static_cast<size_t>(c.height())
                     ? lines.size() - c.height()
                     : 0;
  for (size_t i = first; i < lines.size(); ++i)
    c.put(static_cast<int>(i - first), 0, lines[i]);
}

void TUI::draw_memory(const Fanta::Canvas &c) {
  // As many 16-byte rows as fit: "0x000000  00 11 .. ff  ................"
  int per_row = c.width() >= 10 + 16 * 3 + 17 ? 16 : 8;
  for (int row = 0; row < c.height(); ++row) {
    uint32_t base = mem_offset + static_cast<uint32_t>(row * per_row);
    if (base + per_row > cpu.ram.getSize())
      break;
    std::string hex;
    std::string ascii;
    const uint8_t *p = cpu.ram.from(base);
    for (int j = 0; j < per_row; ++j) {
      hex += std::format("{:02x} ", p[j]);
      ascii += (p[j] >= 0x20 && p[j] < 0x7F) ? static_cast<char>(p[j]) : '.';
    }
    c.put(row, 0, std::format("{}  {} {}", addr(base), hex, ascii),
          COLOR_PAIR(kPairMemory));
  }
}

void TUI::draw_vram(const Fanta::Canvas &c, bool zoomed) {
  if (c.width() <= 0 || c.height() <= 0)
    return;
  const auto *vram = reinterpret_cast<const uint32_t *>(cpu.get_vram());
  bool use_color = (COLORS >= 256) && !ascii_vram;

  double visible_w = 320.0 / zoom_scale;
  double visible_h = 240.0 / zoom_scale;
  double x0 = std::clamp(zoom_cx - visible_w / 2.0, 0.0, 320.0 - visible_w);
  double y0 = std::clamp(zoom_cy - visible_h / 2.0, 0.0, 240.0 - visible_h);

  // Each cell shows the AVERAGE of the pixels it covers, so small objects
  // (a 1-pixel star) still register instead of falling between samples.
  bool any_lit = false;
  for (int y = 0; y < c.height(); ++y) {
    int py0 = static_cast<int>(y0 + y * visible_h / c.height());
    int py1 = std::max(py0 + 1,
                       static_cast<int>(y0 + (y + 1) * visible_h / c.height()));
    for (int x = 0; x < c.width(); ++x) {
      int px0 = static_cast<int>(x0 + x * visible_w / c.width());
      int px1 = std::max(px0 + 1, static_cast<int>(x0 + (x + 1) * visible_w /
                                                         c.width()));
      uint32_t r = 0;
      uint32_t g = 0;
      uint32_t b = 0;
      uint32_t peak = 0;
      int n = 0;
      for (int py = py0; py < std::min(py1, 240); ++py) {
        for (int px = px0; px < std::min(px1, 320); ++px) {
          uint32_t p = vram[py * 320 + px];
          uint32_t pr = (p >> 16) & 0xFF;
          uint32_t pg = (p >> 8) & 0xFF;
          uint32_t pb = p & 0xFF;
          r += pr;
          g += pg;
          b += pb;
          peak = std::max(peak, (pr + pg + pb) / 3);
          ++n;
        }
      }
      if (n == 0)
        continue;
      r /= n;
      g /= n;
      b /= n;
      // Brightness from the average, but never let a lit pixel vanish.
      uint32_t lum = std::max((r + g + b) / 3, peak / 4);
      if (lum > 0)
        any_lit = true;
      if (use_color) {
        int pair = 10 + static_cast<int>((r * 5 / 255) * 36 +
                                         (g * 5 / 255) * 6 + (b * 5 / 255));
        if (lum > 0 && pair == 10)
          pair = 10 + 36 + 6 + 1; // dim but lit: darkest non-black
        c.put(y, x, " ", COLOR_PAIR(pair));
      } else {
        static constexpr std::string_view ramp = " .:-=+*#%@";
        size_t i = lum == 0 ? 0 : std::max<size_t>(1, lum * 9 / 255);
        c.put(y, x, ramp.substr(i, 1));
      }
    }
  }
  if (!any_lit)
    c.put(0, 0, "(blank)", A_DIM);

  if (zoomed) {
    int sx = static_cast<int>((zoom_cx - x0) * c.width() / visible_w);
    int sy = static_cast<int>((zoom_cy - y0) * c.height() / visible_h);
    c.put(sy, sx, "+", COLOR_PAIR(kPairStatus) | A_BOLD | A_REVERSE);
  }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void TUI::handle_input() {
  int ch = getch();
  if (ch == ERR)
    return;
  handle_key(ch);
}

void TUI::handle_key(int ch) {
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
  case 'o': {
    // Cycle the output tab. On terminals too small for the panel, the first
    // press opens it over the work area (ESC closes it).
    auto l = Fanta::Layout::compute(term_w, term_h, layout_kind());
    if (!l.output && !output_overlay) {
      output_overlay = true;
      break;
    }
    output_tab = static_cast<OutputTab>((static_cast<int>(output_tab) + 1) % 4);
    break;
  }
  case 27: // ESC
    output_overlay = false;
    break;
  case '[': // memory tab: back one 16-byte row
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
  case 27: // ESC: leave insert mode
    mode = Mode::NORMAL;
    curs_set(0);
    if (source == Source::FANTA) {
      // Recompiling moves every address, so no live patch: rebuild on demand.
      message = "Edited: :compile (or r) to rebuild, :run to rebuild and run";
    } else {
      assemble_editor(); // live-patch memory with the buffer
    }
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
    if (source == Source::FANTA) { // no mnemonics: indent to the next 4
      do {
        editor.insert(' ');
      } while (editor.x() % 4 != 0);
      break;
    }
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
    } else if (cmd == "compile" || cmd == "c") {
      if (source != Source::FANTA)
        message = "Nothing to compile: :e a .fn file first";
      else
        reset();
    } else if (cmd == "run") {
      if (source != Source::FANTA) {
        message = "Nothing to compile: :e a .fn file first";
      } else {
        reset();
        if (!build_failed)
          is_running_continuously = true;
      }
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
  build_count = 0;
  std::string previous_path = path;
  path = filename;
  if (filename.ends_with(".bin")) {
    auto image = Fanta::Program::readBinary(filename);
    if (!image)
      return (path = previous_path, false);

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
    return (path = previous_path, false);

  if (filename.ends_with(".fn")) {
    source = Source::FANTA;
    assem = Assembler(); // compiled code carries no labels
    asm_errors.clear();
    binary_image.clear();
    editor.set_lines(std::move(*lines));
    reset(); // compile, then fresh CPU with the result
    if (build_failed) {
      // Still open the file so it can be fixed here, but don't leave the
      // previous file's program running underneath it.
      cpu = CPU{};
      return true;
    }
    message = std::format("Loaded {}: compiled {} instructions", filename,
                          binary_image.size());
    return true;
  }

  source = Source::ASM;
  binary_image.clear();
  editor.set_lines(std::move(*lines));
  reset(); // fresh CPU, then assemble the new buffer
  if (asm_errors.empty())
    message = "Loaded " + filename;
  return true;
}

// ---------------------------------------------------------------------------
// Headless screenshots
// ---------------------------------------------------------------------------

namespace {

/// Parses a key script: plain characters, plus <ESC> <CR> <TAB> <BS> <UP>
/// <DOWN> <LEFT> <RIGHT> <FRAME> (<FRAME> = let one UI frame pass, which runs
/// a batch of cycles in continuous mode). Returns -1 for <FRAME>.
auto parse_keys(std::string_view script) -> std::vector<int> {
  static const std::pair<std::string_view, int> names[] = {
      {"ESC", 27},          {"CR", '\n'},          {"TAB", '\t'},
      {"BS", KEY_BACKSPACE}, {"UP", KEY_UP},       {"DOWN", KEY_DOWN},
      {"LEFT", KEY_LEFT},   {"RIGHT", KEY_RIGHT}, {"FRAME", -1},
  };
  std::vector<int> keys;
  for (size_t i = 0; i < script.size(); ++i) {
    if (script[i] == '<') {
      auto close = script.find('>', i);
      if (close != std::string_view::npos) {
        auto name = script.substr(i + 1, close - i - 1);
        auto it = std::ranges::find(names, name, [](auto &p) { return p.first; });
        if (it != std::end(names)) {
          keys.push_back(it->second);
          i = close;
          continue;
        }
      }
    }
    keys.push_back(static_cast<unsigned char>(script[i]));
  }
  return keys;
}

} // namespace

bool TUI::screenshot(const std::string &filename, int cols, int rows,
                     std::string_view keys) {
  std::ofstream out(filename);
  if (!out)
    return false;

  // A real ncurses screen drawing into /dev/null: update() runs exactly as
  // in the terminal, and we read the cells back from stdscr.
  FILE *sink = std::fopen("/dev/null", "w");
  FILE *source = std::fopen("/dev/null", "r");
  char term_name[] = "xterm-256color"; // newterm takes a non-const char*
  SCREEN *screen = (sink && source) ? newterm(term_name, sink, source) : nullptr;
  if (screen == nullptr) {
    if (sink)
      std::fclose(sink);
    if (source)
      std::fclose(source);
    return false;
  }
  set_term(screen);
  configure_ncurses();
  resizeterm(rows, cols);
  ascii_vram = true; // colour blocks would read back as blank cells

  for (int key : parse_keys(keys)) {
    if (key == -1) {
      if (is_running_continuously)
        run_batch();
      continue;
    }
    handle_key(key);
    if (!running)
      break;
  }
  if (is_running_continuously)
    run_batch(); // one frame's worth, like the real loop before drawing
  update();

  int cur_y = 0;
  int cur_x = 0;
  getyx(stdscr, cur_y, cur_x);

  std::string border = "+" + std::string(cols, '-') + "+";
  out << border << "\n";
  std::vector<chtype> cells(cols + 1);
  std::string attrs_report;
  for (int y = 0; y < rows; ++y) {
    mvinchnstr(y, 0, cells.data(), cols);
    std::string text(cols, ' ');
    std::string marks(cols, ' ');
    bool any_mark = false;
    for (int x = 0; x < cols; ++x) {
      char c = static_cast<char>(cells[x] & A_CHARTEXT);
      text[x] = (c >= 0x20 && c < 0x7F) ? c : ' ';
      if (cells[x] & A_REVERSE) {
        marks[x] = 'R';
        any_mark = true;
      } else if (cells[x] & A_BOLD) {
        marks[x] = 'B';
        any_mark = true;
      }
    }
    out << "|" << text << "|\n";
    if (any_mark) {
      marks.erase(marks.find_last_not_of(' ') + 1);
      attrs_report += std::format("{:3} {}\n", y, marks);
    }
  }
  out << border << "\n";
  out << std::format("size {}x{}  cursor at row {}, col {}  mode {}\n", cols,
                     rows, cur_y, cur_x, mode_name(mode));
  if (!attrs_report.empty())
    out << "highlights (R = reverse video, B = bold), by row:\n"
        << attrs_report;

  endwin();
  set_term(nullptr);
  delscreen(screen);
  ncurses_active = false;
  std::fclose(sink);
  std::fclose(source);
  return true;
}
