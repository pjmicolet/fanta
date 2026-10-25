#pragma once
#include "cpu.hpp"
#include "editor.hpp"
#include "program.hpp"
#include "string_assembler.hpp"
#include "trie.hpp"
#include <array>
#include <chrono>
#include <ncurses.h>
#include <string>
#include <vector>

enum class Mode : uint8_t { NORMAL, INSERT, ZOOM, COMMAND };

/// What the editor buffer is, and therefore how a reset rebuilds memory.
enum class Source : uint8_t {
  ASM,    // Editor text is the source of truth; reset reassembles it.
  BINARY, // Editor shows a read-only disassembly; reset restores the image.
};

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

private:
  void update();
  void draw_registers();
  void draw_memory();
  void draw_stack();
  void draw_disassembly();
  void draw_status();
  void draw_editor();
  void draw_vram_preview();
  void draw_serial();
  auto serial_output() -> const std::string &;
  void handle_input();

  void handle_normal_mode(int ch);
  void handle_insert_mode(int ch);
  void handle_zoom_mode(int ch);
  void handle_command_mode(int ch);

  /// Assemble the editor into memory (ASM source only).
  void assemble_editor();
  void step();
  void run_batch();
  void track_changed_registers(const std::array<std::uint32_t, 17> &before);
  void reset_zoom();

  CPU &cpu;
  bool running = true;
  bool ncurses_active = false;
  uint32_t mem_offset = 0;
  bool is_running_continuously = false;
  bool show_serial = false; // bottom-right pane: VRAM monitor or serial

  int term_w = 80;
  int term_h = 24;

  Mode mode = Mode::NORMAL;
  Source source = Source::ASM;
  Editor editor;
  int editor_scroll = 0;
  std::vector<uint32_t> binary_image;
  std::vector<Fanta::Program::AsmError> asm_errors;

  std::string command_buffer; // ":..." while in COMMAND mode
  std::string message;        // last status message, shown in NORMAL mode

  Assembler assem;
  Trie trie;
  void init_trie();

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
