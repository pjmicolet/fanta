#pragma once
#include "layout.hpp"
#include <ncurses.h>
#include <string>
#include <string_view>

/**
 * @brief Drawing bound to one pane's rectangle.
 *
 * @details Every write is clipped to the rectangle, so a pane can never
 * paint over its neighbours (spec §3.3). Coordinates are relative to the
 * rectangle. A framed canvas draws a border with a title and then hands out
 * its inner area.
 */
namespace Fanta {

class Canvas {
public:
  explicit Canvas(Layout::Rect r) : r_(r) {}

  auto width() const -> int { return r_.w; }
  auto height() const -> int { return r_.h; }
  auto rect() const -> Layout::Rect { return r_; }

  /// Blank the whole area (so nothing from a previous frame shows through).
  auto clear() const -> void {
    std::string blank(std::max(r_.w, 0), ' ');
    for (int y = 0; y < r_.h; ++y)
      mvaddnstr(r_.y + y, r_.x, blank.c_str(), r_.w);
  }

  /// Text at (row, col); clipped, and marked with '~' when cut at the edge.
  auto put(int row, int col, std::string_view text,
           attr_t attrs = A_NORMAL) const -> void {
    if (row < 0 || row >= r_.h)
      return;
    auto visible = Layout::clip(text, col, r_.w);
    if (visible.empty())
      return;
    std::string s{visible};
    if (Layout::is_cut(text, col, r_.w))
      s.back() = '~';
    attron(attrs);
    mvaddnstr(r_.y + row, r_.x + std::max(col, 0), s.c_str(),
              static_cast<int>(s.size()));
    attroff(attrs);
  }

  /// A whole row padded to the width (for highlighted lines).
  auto put_line(int row, std::string_view text, attr_t attrs = A_NORMAL) const
      -> void {
    std::string s{text};
    if (static_cast<int>(s.size()) < r_.w)
      s.resize(r_.w, ' ');
    put(row, 0, s, attrs);
  }

  /// Border with a title in the top edge; returns the inner canvas.
  auto frame(std::string_view title, attr_t attrs = A_NORMAL) const
      -> Canvas {
    if (r_.w < 2 || r_.h < 2)
      return Canvas{{r_.x, r_.y, 0, 0}};
    std::string top = "+" + std::string(r_.w - 2, '-') + "+";
    std::string mid = "|" + std::string(r_.w - 2, ' ') + "|";
    put(0, 0, top, attrs);
    for (int y = 1; y < r_.h - 1; ++y)
      put(y, 0, mid, attrs);
    put(r_.h - 1, 0, top, attrs);
    if (!title.empty()) {
      std::string t = "- " + std::string{title} + " ";
      put(0, 1, std::string_view{t}.substr(
                    0, std::min<size_t>(t.size(), std::max(r_.w - 3, 0))),
          attrs | A_BOLD);
    }
    return Canvas{r_.inner()};
  }

  /// Screen position of a point inside the canvas (for the cursor).
  auto move_cursor(int row, int col) const -> void {
    move(r_.y + std::clamp(row, 0, std::max(r_.h - 1, 0)),
         r_.x + std::clamp(col, 0, std::max(r_.w - 1, 0)));
  }

private:
  Layout::Rect r_;
};

} // namespace Fanta
