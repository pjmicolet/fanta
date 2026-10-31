#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

/**
 * @brief Screen layout for the TUI as a pure function (docs/tui_spec.md §3).
 *
 * @details No ncurses here: the layout is computed from the terminal size and
 * the session kind, and every pane then draws only inside its rectangle (see
 * canvas.hpp). That makes overlapping panes impossible by construction, and
 * lets test/tui.cpp check the layout at many sizes.
 */
namespace Fanta::Layout {

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  auto empty() const -> bool { return w <= 0 || h <= 0; }
  auto right() const -> int { return x + w; }  // one past the last column
  auto bottom() const -> int { return y + h; } // one past the last row
  /// The area inside a one-character border.
  auto inner() const -> Rect { return {x + 1, y + 1, w - 2, h - 2}; }
  auto overlaps(const Rect &o) const -> bool {
    return !empty() && !o.empty() && x < o.right() && o.x < right() &&
           y < o.bottom() && o.y < bottom();
  }
  auto inside(int cols, int rows) const -> bool {
    return x >= 0 && y >= 0 && right() <= cols && bottom() <= rows;
  }
};

/// SOURCE: Fanta source + derived ASM. ASM / BINARY: one code pane.
enum class Kind : uint8_t { SOURCE, ASM, BINARY };

/// How the registers pane is drawn, chosen by the space it gets.
enum class RegisterForm : uint8_t {
  FOUR_COLUMNS, // 5 rows inside a border (SOURCE, bottom strip)
  TWO_COLUMNS,  // 10 rows inside a border (ASM, right column)
  COMPACT,      // 3 rows, no border (small terminals)
};

struct Layout {
  Rect status; // top row
  Rect hint;   // bottom row (the command line while typing ':')
  Rect code;   // SOURCE: the Fanta source. ASM/BINARY: the only code pane.
  std::optional<Rect> derived; // SOURCE only: read-only ASM of the build
  std::optional<Rect> registers;
  RegisterForm register_form = RegisterForm::COMPACT;
  std::optional<Rect> stack;
  std::optional<Rect> output;           // tabbed panel
  std::optional<Rect> output_secondary; // a second tab, side by side (wide)

  /// Every pane that is present, for overlap checks.
  auto panes() const -> std::array<std::optional<Rect>, 8> {
    return {status, hint,  code,   derived,
            registers, stack, output, output_secondary};
  }
};

inline constexpr int kMinCols = 80;
inline constexpr int kMinRows = 24;

struct Options {
  bool output_visible = true; // the user can hide the output panel
};

/**
 * @brief Places every pane for a `cols` x `rows` terminal.
 *
 * @details Panes are dropped in the order of spec §3.3 as space runs out:
 * output panel, stack, full registers (to the compact form), and in SOURCE
 * the derived ASM pane. Status, hint and the main code pane always exist.
 * Sizes below 80x24 are laid out as if they were 80x24 and clipped by the
 * canvas; the caller can warn that the terminal is too small.
 */
inline auto compute(int cols, int rows, Kind kind, Options opts = {})
    -> Layout {
  cols = std::max(cols, kMinCols);
  rows = std::max(rows, kMinRows);

  Layout l;
  l.status = {0, 0, cols, 1};
  l.hint = {0, rows - 1, cols, 1};
  int top = 1;
  int bottom = rows - 1; // one past the last body row

  // 1. Output panel: first to go. Needs room left for code + registers.
  if (opts.output_visible && rows >= 32) {
    int h = std::clamp((bottom - top) / 4, 6, 16);
    Rect out{0, bottom - h, cols, h};
    if (cols >= 160) { // wide: two tabs side by side
      int half = cols / 2;
      l.output = Rect{0, out.y, half, h};
      l.output_secondary = Rect{half, out.y, cols - half, h};
    } else {
      l.output = out;
    }
    bottom = out.y;
  }

  if (kind == Kind::SOURCE) {
    // 2./3. Registers + stack strip under the code panes.
    constexpr int kStripH = 7;     // 5 register rows + border
    constexpr int kMinCodeRows = 16; // code panes keep at least this many
    bool full_strip = bottom - top >= kStripH + kMinCodeRows;
    if (full_strip) {
      int regs_w = std::min(cols, 66);
      bool with_stack = cols - regs_w >= 24;
      l.registers = Rect{0, bottom - kStripH, with_stack ? regs_w : cols,
                         kStripH};
      l.register_form = RegisterForm::FOUR_COLUMNS;
      if (with_stack)
        l.stack = Rect{regs_w, bottom - kStripH, cols - regs_w, kStripH};
      bottom -= kStripH;
    } else {
      l.registers = Rect{0, bottom - 3, cols, 3};
      l.register_form = RegisterForm::COMPACT;
      bottom -= 3;
    }

    // 4. Code panes side by side; the derived pane goes last.
    Rect work{0, top, cols, bottom - top};
    int source_w = std::max(work.w * 45 / 100, 34);
    if (work.w - source_w >= 30) {
      l.code = Rect{0, top, source_w, work.h};
      l.derived = Rect{source_w, top, work.w - source_w, work.h};
    } else {
      l.code = work;
    }
    return l;
  }

  // ASM / BINARY: one code pane; registers and stack in a right column when
  // there is room, otherwise compact registers under the code.
  constexpr int kColumnW = 32;
  constexpr int kTwoColRegsH = 12; // 10 rows + border
  if (cols >= 100 && bottom - top >= kTwoColRegsH) {
    int col_x = cols - kColumnW;
    l.code = Rect{0, top, col_x, bottom - top};
    l.registers = Rect{col_x, top, kColumnW, kTwoColRegsH};
    l.register_form = RegisterForm::TWO_COLUMNS;
    if (bottom - top - kTwoColRegsH >= 4)
      l.stack = Rect{col_x, top + kTwoColRegsH, kColumnW,
                     bottom - top - kTwoColRegsH};
  } else {
    l.registers = Rect{0, bottom - 3, cols, 3};
    l.register_form = RegisterForm::COMPACT;
    l.code = Rect{0, top, cols, bottom - 3 - top};
  }
  return l;
}

/**
 * @brief The part of `text` that is visible when drawn at column `col` of a
 * `width`-wide area. Text that runs past the edge ends in '~' so it is
 * visibly cut, never silently.
 */
inline auto clip(std::string_view text, int col, int width)
    -> std::string_view {
  if (col >= width || width <= 0)
    return {};
  if (col < 0) { // starts left of the area: drop the hidden part
    if (static_cast<int>(text.size()) <= -col)
      return {};
    text.remove_prefix(-col);
    col = 0;
  }
  auto room = static_cast<size_t>(width - col);
  return text.size() <= room ? text : text.substr(0, room);
}

inline auto is_cut(std::string_view text, int col, int width) -> bool {
  return col < width && static_cast<int>(text.size()) > width - std::max(col, 0);
}

} // namespace Fanta::Layout
