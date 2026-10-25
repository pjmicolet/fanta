#pragma once
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Lays a serial byte stream out as screen lines, the way a dumb
 * terminal would. No ncurses, so it is unit tested (test/tui.cpp).
 *
 * @details Handled: printable bytes, '\n' (next line), '\r' (column 0),
 * '\b' (back one column), '\t' (next multiple of 8). Lines wrap at `width`
 * with a deferred wrap: a full line followed by '\n' is one line, not two.
 * Any other byte shows as '.'. Escape sequences (cursor positioning) are not
 * interpreted yet.
 */
namespace Fanta::Terminal {

inline auto layout(std::string_view stream, std::size_t width)
    -> std::vector<std::string> {
  std::vector<std::string> lines(1);
  if (width == 0)
    return lines;
  std::size_t col = 0;

  auto put = [&](char c) {
    if (col == width) { // deferred wrap: only when there's more to write
      lines.emplace_back();
      col = 0;
    }
    auto &line = lines.back();
    if (col < line.size())
      line[col] = c; // overwrite after '\r' or '\b'
    else
      line.push_back(c);
    ++col;
  };

  for (char c : stream) {
    switch (c) {
    case '\n':
      lines.emplace_back();
      col = 0;
      break;
    case '\r':
      col = 0;
      break;
    case '\b':
      if (col > 0)
        --col;
      break;
    case '\t':
      do {
        put(' ');
      } while (col % 8 != 0 && col < width);
      break;
    default:
      put((c >= 0x20 && c < 0x7F) ? c : '.');
      break;
    }
  }
  return lines;
}

} // namespace Fanta::Terminal
