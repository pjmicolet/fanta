#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Line-based text buffer with a cursor, independent of ncurses.
 *
 * @details Shared by every editing mode (assembly today, Fanta source next).
 * A read-only editor ignores every edit but still moves its cursor; used for
 * programs whose text is a view (disassembly of a binary) rather than the
 * source of truth.
 */
class Editor {
public:
  Editor() : lines_{""} {}

  [[nodiscard]] auto lines() const -> const std::vector<std::string> & { return lines_; }
  [[nodiscard]] auto line() const -> const std::string & { return lines_[y_]; }
  [[nodiscard]] auto x() const -> int { return x_; }
  [[nodiscard]] auto y() const -> int { return y_; }
  [[nodiscard]] auto read_only() const -> bool { return read_only_; }
  /// Bumped by every edit (and by set_lines). Comparing against a saved
  /// value tells whether the text changed since then (unsaved, stale build).
  [[nodiscard]] auto version() const -> uint64_t { return version_; }
  /// Bumped only by edits that add or remove lines (newline, joining lines,
  /// set_lines). In assembly, those are the edits that can move addresses.
  [[nodiscard]] auto structure_version() const -> uint64_t { return structure_version_; }

  auto set_lines(std::vector<std::string> lines, bool read_only = false)
      -> void {
    lines_ = std::move(lines);
    if (lines_.empty())
      lines_.emplace_back();
    read_only_ = read_only;
    x_ = 0;
    y_ = 0;
    ++version_;
    ++structure_version_;
  }

  [[nodiscard]] auto text() const -> std::string {
    std::string out;
    for (const auto &l : lines_)
      out += l + "\n";
    return out;
  }

  // --- Cursor movement ---
  auto left() -> void {
    if (x_ > 0)
      --x_;
  }
  auto right() -> void {
    if (x_ < len())
      ++x_;
  }
  auto up() -> void {
    if (y_ > 0) {
      --y_;
      clamp_x();
    }
  }
  auto down() -> void {
    if (y_ < static_cast<int>(lines_.size()) - 1) {
      ++y_;
      clamp_x();
    }
  }

  /// Puts the cursor on line `y` (clamped), keeping the column if it fits.
  auto goto_line(int y) -> void {
    y_ = std::clamp(y, 0, static_cast<int>(lines_.size()) - 1);
    clamp_x();
  }

  // --- Edits (no-ops when read-only) ---
  auto insert(char c) -> void {
    if (read_only_)
      return;
    lines_[y_].insert(x_, 1, c);
    ++x_;
    ++version_;
  }

  /// Deletes before the cursor, joining with the previous line at column 0.
  auto backspace() -> void {
    if (read_only_)
      return;
    if (x_ > 0) {
      lines_[y_].erase(x_ - 1, 1);
      --x_;
      ++version_;
    } else if (y_ > 0) {
      x_ = static_cast<int>(lines_[y_ - 1].size());
      lines_[y_ - 1] += lines_[y_];
      lines_.erase(lines_.begin() + y_);
      --y_;
      ++version_;
      ++structure_version_;
    }
  }

  /// Deletes under the cursor (vim's `x`).
  auto erase_at_cursor() -> void {
    if (read_only_ || x_ >= len())
      return;
    lines_[y_].erase(x_, 1);
    ++version_;
  }

  /// Splits the line at the cursor. The new line starts with the indentation
  /// in front of the cursor (auto-indent), plus one level when `braces` is
  /// set and the text before the cursor ends in `{`.
  auto newline(bool braces = false) -> void {
    if (read_only_)
      return;
    const std::string &l = lines_[y_];
    std::string head = l.substr(0, x_);
    std::string indent = new_line_indent(head, braces);
    std::string rest = l.substr(x_);
    if (!indent.empty()) // the indent replaces the moved text's own
      rest.erase(0, std::min(rest.find_first_not_of(" \t"), rest.size()));
    lines_[y_] = head;
    lines_.insert(lines_.begin() + y_ + 1, indent + rest);
    ++y_;
    x_ = static_cast<int>(indent.size());
    ++version_;
    ++structure_version_;
  }

  /// Opens an empty line below (vim's `o`) or above (`O`) and puts the
  /// cursor at its indentation: the current line's, plus one level (when
  /// `braces` is set) below a line ending in `{` or above one starting `}`.
  auto open_line(bool above, bool braces = false) -> void {
    if (read_only_)
      return;
    const std::string &l = lines_[y_];
    std::string indent = above ? indent_of(l) : new_line_indent(l, braces);
    if (above && braces && l.find_first_not_of(" \t") != std::string::npos &&
        l[l.find_first_not_of(" \t")] == '}')
      indent.append(kIndent, ' ');
    if (!above)
      ++y_;
    lines_.insert(lines_.begin() + y_, indent);
    x_ = static_cast<int>(indent.size());
    ++version_;
    ++structure_version_;
  }

  /// Removes one indentation level (a tab, or up to kIndent spaces) from the
  /// start of the line; the cursor stays on the same character.
  auto dedent() -> void {
    if (read_only_)
      return;
    auto &l = lines_[y_];
    size_t n = 0;
    if (!l.empty() && l[0] == '\t')
      n = 1;
    else
      while (n < kIndent && n < l.size() && l[n] == ' ')
        ++n;
    if (n == 0)
      return;
    l.erase(0, n);
    x_ = std::max(0, x_ - static_cast<int>(n));
    ++version_;
  }

  /// True when only whitespace precedes the cursor.
  [[nodiscard]] auto at_indent() const -> bool {
    return lines_[y_].find_first_not_of(" \t") >= static_cast<size_t>(x_);
  }

  static constexpr size_t kIndent = 4;

  /// Replaces the first word of the current line, keeping its indentation,
  /// and puts the cursor after it plus one space (TAB completion).
  auto replace_first_word(const std::string &word) -> void {
    if (read_only_)
      return;
    auto &l = lines_[y_];
    auto start = l.find_first_not_of(" \t");
    if (start == std::string::npos)
      start = l.size();
    auto end = l.find_first_of(" \t", start);
    std::string tail = end == std::string::npos ? "" : l.substr(end);
    l = l.substr(0, start) + word + (tail.empty() ? " " : tail);
    x_ = static_cast<int>(start + word.size() + 1);
    clamp_x();
    ++version_;
  }

  /// Trims trailing whitespace on every line; keeps the cursor in range.
  /// Not an edit: doesn't bump the version (it runs before every build).
  auto trim_trailing() -> void {
    for (auto &l : lines_)
      l.erase(l.find_last_not_of(" \t\r\n") + 1);
    clamp_x();
  }

private:
  [[nodiscard]] static auto indent_of(const std::string &l) -> std::string {
    return l.substr(0, std::min(l.find_first_not_of(" \t"), l.size()));
  }
  /// Indentation for a line that follows `prev`.
  [[nodiscard]] static auto new_line_indent(const std::string &prev, bool braces)
      -> std::string {
    std::string indent = indent_of(prev);
    auto last = prev.find_last_not_of(" \t");
    if (braces && last != std::string::npos && prev[last] == '{')
      indent.append(kIndent, ' ');
    return indent;
  }

  [[nodiscard]] auto len() const -> int { return static_cast<int>(lines_[y_].size()); }
  auto clamp_x() -> void { x_ = std::min(x_, len()); }

  std::vector<std::string> lines_;
  int x_ = 0;
  int y_ = 0;
  bool read_only_ = false;
  uint64_t version_ = 0;
  uint64_t structure_version_ = 0;
};
