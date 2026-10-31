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

enum class Mode : uint8_t { NORMAL, INSERT, COMMAND };

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
  /// F6: fresh CPU with the current image. Never rebuilds.
  void reset();
  /// F9: compile (.fn) or assemble (.asm), then restart. A failed compile
  /// keeps the previous image loaded. Returns whether the build succeeded.
  bool build();
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
  /// Append the current message to the Build log (once).
  void log_message();
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
  void draw_disasm_view(const Fanta::Canvas &c, std::string_view title,
                        bool dimmed = false);
  void draw_registers(const Fanta::Canvas &c,
                      Fanta::Layout::RegisterForm form);
  void draw_stack(const Fanta::Canvas &c);
  void draw_output(const Fanta::Canvas &c, OutputTab tab, bool overlay = false);
  void draw_serial(const Fanta::Canvas &c);
  void draw_memory(const Fanta::Canvas &c);
  void draw_vram(const Fanta::Canvas &c, bool zoomed);
  auto serial_output() -> const std::string &;
  void handle_input();

  void handle_normal_mode(int ch);
  void handle_insert_mode(int ch);
  void handle_overlay_key(int ch); // VRAM zoom/pan in the output overlay
  void handle_command_mode(int ch);

  /// Assemble the editor into memory (ASM source only).
  void assemble_editor();
  /// Compile the editor (FANTA source only). On success the result becomes
  /// binary_image; on failure memory is left as it was.
  bool compile_editor();
  void step();
  void run_batch();
  void fresh_cpu();
  void run_pause();   // F5
  void step_over();   // F8
  void cycle_output(); // F10
  /// Execution keys work in every mode but COMMAND (spec §4.1).
  bool handle_fkey(int ch);
  void draw_help(const Fanta::Canvas &c);

  // Session model (spec §2).
  /// Edited since the last :w / load.
  auto dirty() const -> bool;
  /// The source changed since the image in memory was built from it.
  auto stale() const -> bool;
  /// After a build or a live patch: memory now matches the editor text.
  void mark_built();
  bool write_file(const std::string &target);
  void eject(bool force);
  void run_command(const std::string &cmd);
  void track_changed_registers(const std::array<std::uint32_t, 17> &before);
  /// In an ASM session, move the editor cursor to the line at PC.
  void follow_pc();
  /// Switch the output tab to the device the program last wrote to.
  void follow_output();
  auto vram_hash() -> std::size_t;
  void reset_zoom();

  CPU &cpu;
  bool running = true;
  bool ncurses_active = false;
  uint32_t mem_offset = 0;
  bool is_running_continuously = false;
  bool ascii_vram = false; // draw VRAM with a character ramp (screenshots)
  OutputTab output_tab = OutputTab::SERIAL;
  bool output_overlay = false; // output over the work area ('z', small terms)
  bool tab_pinned = false;     // user picked a tab: stop following devices
  std::size_t seen_serial = 0; // serial bytes at the last check
  std::size_t seen_vram = 0;   // VRAM hash at the last check
  bool help_open = false;      // '?' / :help overlay; any key closes it
  std::optional<uint32_t> run_target; // step over: stop at this PC...
  uint32_t run_target_sp = 0;         // ...with this SP (the call returned)

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
  int image_build = 0;       // build number of the image in memory (0: none)
  bool code_moved = false;   // ASM: lines added/removed before code since build
  bool message_sticky = false; // build error: message stays until next build
  uint64_t saved_version = 0; // editor.version() at the last save / load
  uint64_t built_version = 0; // editor.version() the image was built from
  uint64_t built_structure = 0;          // editor.structure_version() likewise
  std::vector<std::string> built_lines;  // ASM: the text memory matches
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


  // Zooming
  double zoom_scale = 1.0;
  double zoom_cx = 160.0;
  double zoom_cy = 120.0;
};
