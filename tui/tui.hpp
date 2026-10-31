#pragma once
#include "cpu.hpp"
#include "canvas.hpp"
#include "editor.hpp"
#include "layout.hpp"
#include "program.hpp"
#include "string_assembler.hpp"
#include "trie.hpp"
#include <array>
#include <bitset>
#include <optional>
#include <chrono>
#include <ncurses.h>
#include <string>
#include <vector>

enum class Mode : uint8_t { NORMAL, INSERT, ZOOM, COMMAND };

/// What the editor buffer is, and therefore how a reset rebuilds memory.
enum class Source : uint8_t {
  ASM,    // Editor text is the source of truth; reset reassembles it.
  BINARY, // Editor shows a read-only disassembly; reset restores the image.
  FANTA,  // Editor is Fanta source; reset (or :compile) recompiles it.
};

/// Tabs of the output panel (spec §3.2).
enum class OutputTab : uint8_t { SERIAL, VRAM, MEMORY, LOG };

class TUI {
public:
  TUI(CPU &cpu);
  ~TUI();

  void init_ncurses();
  void run();
  void dump_state(const std::string &filename);
  std::string disassemble(uint32_t addr);
  bool load_file(const std::string &filename);
  /// Fresh CPU, then rebuild memory from the current source.
  void reset();
  /// Process one key exactly as if it had been typed.
  void handle_key(int ch);
  /**
   * Renders the real screen headlessly at cols x rows after feeding `keys`
   * (see parse_keys in tui.cpp for the <ESC>/<CR>/<FRAME> syntax), and writes
   * it as text plus a list of highlighted cells. For checking the layout
   * without a terminal.
   */
  bool screenshot(const std::string &filename, int cols, int rows,
                  std::string_view keys = "");

private:
  void update();
  void configure_ncurses();
  auto layout_kind() const -> Fanta::Layout::Kind;
  /// PC as shown to the user: on the HALT itself once halted (spec §6.1).
  auto display_pc() const -> uint32_t;

  // Panes; each draws only inside its canvas.
  void draw_status_line(const Fanta::Canvas &c);
  void draw_hint_bar(const Fanta::Canvas &c);
  void draw_source(const Fanta::Canvas &c);
  void draw_asm_editor(const Fanta::Canvas &c);
  void draw_code_lines(const Fanta::Canvas &c, bool show_addresses,
                       bool editable);
  void draw_disasm_view(const Fanta::Canvas &c, std::string_view title);
  void draw_registers(const Fanta::Canvas &c,
                      Fanta::Layout::RegisterForm form);
  void draw_stack(const Fanta::Canvas &c);
  void draw_output(const Fanta::Canvas &c, OutputTab tab);
  void draw_serial(const Fanta::Canvas &c);
  void draw_memory(const Fanta::Canvas &c);
  void draw_vram(const Fanta::Canvas &c, bool zoomed);
  auto serial_output() -> const std::string &;
  void handle_input();

  void handle_normal_mode(int ch);
  void handle_insert_mode(int ch);
  void handle_zoom_mode(int ch);
  void handle_command_mode(int ch);

  /// Assemble the editor into memory (ASM source only).
  void assemble_editor();
  /// Compile the editor (FANTA source only). On success the result becomes
  /// binary_image; on failure memory is left as it was.
  bool compile_editor();
  void step();
  void run_batch();
  void track_changed_registers(const std::array<std::uint32_t, 17> &before);
  /// In an ASM session, move the editor cursor to the line at PC.
  void follow_pc();
  void reset_zoom();

  CPU &cpu;
  bool running = true;
  bool ncurses_active = false;
  uint32_t mem_offset = 0;
  bool is_running_continuously = false;
  bool ascii_vram = false; // draw VRAM with a character ramp (screenshots)
  OutputTab output_tab = OutputTab::SERIAL;
  bool output_overlay = false; // small terminals: output over the work area

  int term_w = 80;
  int term_h = 24;

  Mode mode = Mode::NORMAL;
  Source source = Source::ASM;
  Editor editor;
  int editor_scroll = 0;
  std::vector<uint32_t> binary_image;
  std::vector<Fanta::Program::AsmError> asm_errors;
  bool build_failed = false; // last compile failed (message holds why)
  int build_count = 0;       // compiles attempted this session
  std::string path;          // file the session was opened from

  std::string command_buffer; // ":..." while in COMMAND mode
  std::string message;        // last status message, shown in NORMAL mode
  std::vector<std::string> build_log; // every message, for the Build log tab
  std::string last_logged;
  std::optional<std::pair<int, int>> cursor_at; // editor cursor, screen row/col

  Assembler assem;
  Trie trie;
  void init_trie();

  std::bitset<17> changed_regs; // registers changed by the last step/batch

  // IPS Monitoring
  uint64_t total_cycles = 0;
  double current_ips = 0;
  std::chrono::steady_clock::time_point last_ips_time;

  // Zooming
  double zoom_scale = 1.0;
  double zoom_cx = 160.0;
  double zoom_cy = 120.0;
};
