#include "tui.hpp"
#include "cpu.hpp"
#include <cstdio>
#include <iostream>
#include <string>

/**
 * fanta-tui [file] [--dump out.txt] [--screen out.txt [--size COLSxROWS]
 *           [--keys "script"]]
 *
 * --dump    writes machine state (registers, disassembly, editor, serial).
 * --screen  writes what the terminal would show, rendered headlessly.
 *           --keys feeds keystrokes first, e.g. "c<FRAME><FRAME>o" or
 *           ":run<CR>"; see parse_keys in tui.cpp.
 */
int main(int argc, char *argv[]) {
  CPU cpu{};

  std::string dump_file;
  std::string screen_file;
  std::string keys;
  std::string load_file;
  int cols = 120;
  int rows = 40;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    bool has_value = i + 1 < argc;
    if (arg == "--dump" && has_value) {
      dump_file = argv[++i];
    } else if (arg == "--screen" && has_value) {
      screen_file = argv[++i];
    } else if (arg == "--keys" && has_value) {
      keys = argv[++i];
    } else if (arg == "--size" && has_value) {
      if (std::sscanf(argv[++i], "%dx%d", &cols, &rows) != 2) {
        std::cerr << "--size expects COLSxROWS, e.g. 120x40\n";
        return 1;
      }
    } else {
      load_file = arg;
    }
  }

  TUI tui(cpu);
  if (!load_file.empty()) {
    tui.load_file(load_file);
  }

  if (!dump_file.empty() || !screen_file.empty()) {
    // Headless: no interactive terminal is touched.
    if (!screen_file.empty() && !tui.screenshot(screen_file, cols, rows, keys)) {
      std::cerr << "Could not render screen to " << screen_file << "\n";
      return 1;
    }
    if (!dump_file.empty())
      tui.dump_state(dump_file);
    return 0;
  }

  tui.init_ncurses();
  tui.run();
  return 0;
}
