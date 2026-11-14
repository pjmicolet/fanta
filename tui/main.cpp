#include "tui.hpp"
#include "cpu.hpp"
#include <charconv>
#include <string_view>
#include <iostream>
#include <string>

/**
 * fanta-tui [file] [--dump out.txt] [--screen out.txt [--size COLSxROWS]
 *           [--keys "script"] [--colours]]
 *
 * --dump    writes machine state (registers, disassembly, editor, serial).
 * --screen  writes what the terminal would show, rendered headlessly.
 *           --keys feeds keystrokes first, e.g. "c<FRAME><FRAME>o" or
 *           ":run<CR>"; see parse_keys in tui.cpp.
 *           --colours adds the colour pair of every cell (syntax groups).
 */
// Parses "COLSxROWS" (e.g. 120x40). Rejects trailing text, overflow and
// non-positive sizes, which sscanf would accept.
static auto parse_size(std::string_view s, int &cols, int &rows) -> bool {
  auto x = s.find('x');
  if (x == std::string_view::npos)
    return false;
  auto number = [](std::string_view part, int &out) {
    const char *end = part.data() + part.size();
    auto [ptr, ec] = std::from_chars(part.data(), end, out);
    return ec == std::errc{} && ptr == end && out > 0;
  };
  return number(s.substr(0, x), cols) && number(s.substr(x + 1), rows);
}

int main(int argc, char *argv[]) try {
  CPU cpu{};

  std::string dump_file;
  std::string screen_file;
  std::string keys;
  bool colours = false;
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
    } else if (arg == "--colours") {
      colours = true;
    } else if (arg == "--keys" && has_value) {
      keys = argv[++i];
    } else if (arg == "--size" && has_value) {
      if (!parse_size(argv[++i], cols, rows)) {
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
    if (!screen_file.empty() && !tui.screenshot(screen_file, cols, rows, keys, colours)) {
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
} catch (const std::exception &e) {
  // tui was destroyed during unwinding, so ncurses has already been ended.
  std::cerr << "fanta-tui: error: " << e.what() << "\n";
  return 1;
}
