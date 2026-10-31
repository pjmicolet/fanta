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

  auto lines() const -> const std::vector<std::string> & { return lines_; }
  auto line() const -> const std::string & { return lines_[y_]; }
  auto x() const -> int { return x_; }
  auto y() const -> int { return y_; }
  auto read_only() const -> bool { return read_only_; }
  /// Bumped by every edit (and by set_lines). Comparing against a saved
  /// value tells whether the text changed since then (unsaved, stale build).
  auto version() const -> uint64_t { return version_; }
  /// Bumped only by edits that add or remove lines (newline, joining lines,
  /// set_lines). In assembly, those are the edits that can move addresses.
  auto structure_version() const -> uint64_t { return structure_version_; }

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

  auto newline() -> void {
    if (read_only_)
      return;
    std::string rest = lines_[y_].substr(x_);
    lines_[y_].resize(x_);
    lines_.insert(lines_.begin() + y_ + 1, rest);
    ++y_;
    x_ = 0;
    ++version_;
    ++structure_version_;
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
  auto len() const -> int { return static_cast<int>(lines_[y_].size()); }
  auto clamp_x() -> void { x_ = std::min(x_, len()); }

  std::vector<std::string> lines_;
  int x_ = 0;
  int y_ = 0;
  bool read_only_ = false;
  uint64_t version_ = 0;
  uint64_t structure_version_ = 0;
};
