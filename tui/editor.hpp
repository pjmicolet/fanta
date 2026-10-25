#pragma once
#include <algorithm>
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

  auto lines() const -> const std::vector<std::string> & { return lines_; }
  auto line() const -> const std::string & { return lines_[y_]; }
  auto x() const -> int { return x_; }
  auto y() const -> int { return y_; }
  auto read_only() const -> bool { return read_only_; }

  auto set_lines(std::vector<std::string> lines, bool read_only = false)
      -> void {
    lines_ = std::move(lines);
    if (lines_.empty())
      lines_.emplace_back();
    read_only_ = read_only;
    x_ = 0;
    y_ = 0;
  }

  auto text() const -> std::string {
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

  // --- Edits (no-ops when read-only) ---
  auto insert(char c) -> void {
    if (read_only_)
      return;
    lines_[y_].insert(x_, 1, c);
    ++x_;
  }

  /// Deletes before the cursor, joining with the previous line at column 0.
  auto backspace() -> void {
    if (read_only_)
      return;
    if (x_ > 0) {
      lines_[y_].erase(x_ - 1, 1);
      --x_;
    } else if (y_ > 0) {
      x_ = static_cast<int>(lines_[y_ - 1].size());
      lines_[y_ - 1] += lines_[y_];
      lines_.erase(lines_.begin() + y_);
      --y_;
    }
  }

  /// Deletes under the cursor (vim's `x`).
  auto erase_at_cursor() -> void {
    if (read_only_ || x_ >= len())
      return;
    lines_[y_].erase(x_, 1);
  }

  auto newline() -> void {
    if (read_only_)
      return;
    std::string rest = lines_[y_].substr(x_);
    lines_[y_].resize(x_);
    lines_.insert(lines_.begin() + y_ + 1, rest);
    ++y_;
    x_ = 0;
  }

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
  }

  /// Trims trailing whitespace on every line; keeps the cursor in range.
  auto trim_trailing() -> void {
    for (auto &l : lines_)
      l.erase(l.find_last_not_of(" \t\r\n") + 1);
    clamp_x();
  }

private:
  auto len() const -> int { return static_cast<int>(lines_[y_].size()); }
  auto clamp_x() -> void { x_ = std::min(x_, len()); }

  std::vector<std::string> lines_;
  int x_ = 0;
  int y_ = 0;
  bool read_only_ = false;
};
