#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

/**
 * @brief Syntax highlighting driven by `.syntax` files (spec §4.3).
 *
 * @details A definition is a list of rules tried in file order at each
 * position of a line; the first that matches colours its text as a group.
 * Per-line only: nothing carries over between lines. Independent of the
 * compiler's lexer so half-typed code still highlights.
 */
namespace Fanta::Syntax {

enum class Group : uint8_t {
  None,
  Keyword,
  Type,
  String,
  Number,
  Comment,
  Label,
  Register,
  Mnemonic,
  Directive,
  Operator,
  Error,
};

inline constexpr auto kGroupNames = std::to_array<std::string_view>(
    {"none", "keyword", "type", "string", "number", "comment", "label",
     "register", "mnemonic", "directive", "operator", "error"});

/// A run of `len` characters from column `col` in one group.
struct Span {
  int col;
  int len;
  Group group;
};

namespace detail {
inline auto is_word(char c) -> bool {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
inline auto upper(std::string_view s) -> std::string {
  std::string out{s};
  std::ranges::transform(out, out.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return out;
}
} // namespace detail

/**
 * @brief A tiny anchored pattern: literals, `.`, `[a-z_]` / `[^...]`
 * classes, `\` escapes, each optionally followed by `*`, `+` or `?`.
 * Greedy with backtracking; no groups or alternation.
 */
class Pattern {
public:
  explicit Pattern(std::string_view src) {
    for (size_t i = 0; i < src.size();) {
      Atom a;
      char c = src[i++];
      if (c == '.') {
        a.set.set();
      } else if (c == '[') {
        bool negate = i < src.size() && src[i] == '^';
        if (negate)
          ++i;
        bool closed = false;
        while (i < src.size()) {
          char lo = src[i++];
          if (lo == ']') {
            closed = true;
            break;
          }
          if (lo == '\\' && i < src.size())
            lo = src[i++];
          char hi = lo;
          if (i + 1 < src.size() && src[i] == '-' && src[i + 1] != ']') {
            hi = src[i + 1];
            i += 2;
          }
          for (int ch = static_cast<unsigned char>(lo);
               ch <= static_cast<unsigned char>(hi); ++ch)
            a.set.set(static_cast<size_t>(ch));
        }
        if (!closed)
          throw std::runtime_error("unclosed [ in pattern");
        if (negate)
          a.set.flip();
      } else {
        if (c == '\\') {
          if (i >= src.size())
            throw std::runtime_error("pattern ends in \\");
          c = src[i++];
        } else if (c == '*' || c == '+' || c == '?') {
          throw std::runtime_error(std::format("'{}' has nothing to repeat", c));
        }
        a.set.set(static_cast<unsigned char>(c));
      }
      if (i < src.size() &&
          (src[i] == '*' || src[i] == '+' || src[i] == '?'))
        a.quant = src[i++];
      atoms_.push_back(a);
    }
    if (atoms_.empty())
      throw std::runtime_error("empty pattern");
  }

  /// Length of the match starting at `pos`, or 0.
  [[nodiscard]] auto match(std::string_view s, size_t pos) const -> size_t {
    size_t end = match_from(0, s, pos);
    return end == std::string_view::npos ? 0 : end - pos;
  }

private:
  struct Atom {
    std::bitset<256> set;
    char quant = 0;
  };

  [[nodiscard]] auto match_from(size_t ai, std::string_view s, size_t pos) const
      -> size_t {
    if (ai == atoms_.size())
      return pos;
    const Atom &a = atoms_[ai];
    size_t min = (a.quant == '*' || a.quant == '?') ? 0 : 1;
    size_t max = (a.quant == '*' || a.quant == '+') ? s.size() : 1;
    size_t n = 0;
    while (n < max && pos + n < s.size() &&
           a.set.test(static_cast<unsigned char>(s[pos + n])))
      ++n;
    for (;; --n) {
      if (n < min)
        return std::string_view::npos;
      size_t r = match_from(ai + 1, s, pos + n);
      if (r != std::string_view::npos)
        return r;
      if (n == 0)
        return std::string_view::npos;
    }
  }

  std::vector<Atom> atoms_;
};

/**
 * @brief One language's rules, parsed from a `.syntax` file.
 *
 * @details Format, one rule per line (`#` at the start of a line comments):
 * ```
 * extensions .asm .s           files this definition applies to
 * ignorecase                   words match regardless of case
 * comment  ;                   line comment, to the end of the line
 * span     string " " \        open, close, optional escape
 * words    register R0 R1 SP   whole words; @isa = every ISA mnemonic
 * pattern  number #[0-9A-Fa-f]+
 * symbols  operator == != + -  literal tokens, longest first
 * ```
 * `words` and `pattern` never match part of a word.
 */
class Definition {
public:
  /// Parses `text`; throws std::runtime_error naming the line on a mistake.
  /// `isa` fills `@isa` in a `words` rule.
  static auto parse(std::string_view text,
                    const std::vector<std::string> &isa = {}) -> Definition {
    Definition d;
    std::istringstream in{std::string{text}};
    std::string raw;
    int n = 0;
    while (std::getline(in, raw)) {
      ++n;
      try {
        d.parse_line(raw, isa);
      } catch (const std::exception &e) {
        throw std::runtime_error(std::format("line {}: {}", n, e.what()));
      }
    }
    if (d.ignorecase_) // wherever `ignorecase` appeared in the file
      for (auto &r : d.rules_) {
        std::unordered_set<std::string> folded;
        for (const auto &w : r.words)
          folded.insert(detail::upper(w));
        r.words = std::move(folded);
      }
    return d;
  }

  [[nodiscard]] auto extensions() const -> const std::vector<std::string> & {
    return extensions_;
  }

  /// Spans of `line`, in order and non-overlapping, into `out` (cleared
  /// first, so a caller can reuse one vector per frame).
  auto highlight(std::string_view line, std::vector<Span> &out) const -> void {
    out.clear();
    size_t pos = 0;
    while (pos < line.size()) {
      bool word_start = pos == 0 || !detail::is_word(line[pos - 1]) ||
                        !detail::is_word(line[pos]);
      size_t len = 0;
      Group g = Group::None;
      for (const auto &r : rules_) {
        if ((r.kind == Kind::Words || r.kind == Kind::Pattern) && !word_start)
          continue;
        len = match(r, line, pos);
        if (len > 0 && (r.kind == Kind::Words || r.kind == Kind::Pattern) &&
            pos + len < line.size() && detail::is_word(line[pos + len - 1]) &&
            detail::is_word(line[pos + len]))
          len = 0; // stops mid-word
        if (len > 0) {
          g = r.group;
          break;
        }
      }
      if (len > 0) {
        out.push_back({static_cast<int>(pos), static_cast<int>(len), g});
        pos += len;
      } else if (detail::is_word(line[pos])) {
        while (pos < line.size() && detail::is_word(line[pos]))
          ++pos; // no rule matched the word's start: skip all of it
      } else {
        ++pos;
      }
    }
  }

private:
  enum class Kind : uint8_t { Comment, Span, Words, Pattern, Symbols };
  struct Rule {
    Kind kind;
    Group group;
    std::vector<std::string> tokens; // comment/span delimiters, symbols
    std::unordered_set<std::string> words;
    std::optional<Pattern> pattern;
  };

  static auto group_named(std::string_view name) -> Group {
    for (size_t i = 1; i < kGroupNames.size(); ++i)
      if (kGroupNames[i] == name)
        return static_cast<Group>(i);
    throw std::runtime_error(std::format("unknown group '{}'", name));
  }

  auto parse_line(const std::string &raw, const std::vector<std::string> &isa)
      -> void {
    // Whitespace separates arguments; `\ ` keeps a space inside one.
    std::vector<std::string> t;
    std::string w;
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == '\\' && i + 1 < raw.size()) {
        w += raw.substr(i++, 2);
      } else if (std::isspace(static_cast<unsigned char>(raw[i])) != 0) {
        if (!w.empty())
          t.push_back(std::exchange(w, {}));
      } else {
        w += raw[i];
      }
    }
    if (!w.empty())
      t.push_back(w);
    if (t.empty() || t[0][0] == '#')
      return;
    const std::string &kind = t[0];
    auto need = [&](size_t n) {
      if (t.size() < n)
        throw std::runtime_error(std::format("'{}' needs more arguments", kind));
    };
    if (kind == "extensions") {
      extensions_.insert(extensions_.end(), t.begin() + 1, t.end());
    } else if (kind == "ignorecase") {
      ignorecase_ = true;
    } else if (kind == "comment") {
      need(2);
      rules_.push_back({Kind::Comment, Group::Comment, {t[1]}, {}, {}});
    } else if (kind == "span") {
      need(4);
      rules_.push_back({Kind::Span, group_named(t[1]),
                        {t.begin() + 2, t.end()}, {}, {}});
    } else if (kind == "words") {
      need(3);
      Rule r{Kind::Words, group_named(t[1]), {}, {}, {}};
      for (size_t i = 2; i < t.size(); ++i) {
        if (t[i] == "@isa") {
          for (const auto &m : isa)
            r.words.insert(m);
        } else {
          r.words.insert(t[i]);
        }
      }
      rules_.push_back(std::move(r));
    } else if (kind == "pattern") {
      need(3);
      if (t.size() > 3)
        throw std::runtime_error("pattern has a space; write it as '\\ '");
      rules_.push_back({Kind::Pattern, group_named(t[1]), {}, {}, Pattern{t[2]}});
    } else if (kind == "symbols") {
      need(3);
      Rule r{Kind::Symbols, group_named(t[1]), {t.begin() + 2, t.end()}, {}, {}};
      std::ranges::sort(r.tokens, [](const auto &a, const auto &b) {
        return a.size() > b.size();
      });
      rules_.push_back(std::move(r));
    } else {
      throw std::runtime_error(std::format("unknown rule '{}'", kind));
    }
  }

  [[nodiscard]] auto match(const Rule &r, std::string_view s, size_t pos) const
      -> size_t {
    auto rest = s.substr(pos);
    switch (r.kind) {
    case Kind::Comment:
      return rest.starts_with(r.tokens[0]) ? rest.size() : 0;
    case Kind::Span: {
      const auto &open = r.tokens[0];
      const auto &close = r.tokens[1];
      if (!rest.starts_with(open))
        return 0;
      std::string_view esc;
      if (r.tokens.size() > 2)
        esc = r.tokens[2];
      for (size_t i = open.size(); i < rest.size();) {
        if (!esc.empty() && rest.substr(i).starts_with(esc)) {
          i += esc.size() + 1; // skip the escaped character
          continue;
        }
        if (rest.substr(i).starts_with(close))
          return i + close.size();
        ++i;
      }
      return rest.size(); // unterminated: to the end of the line
    }
    case Kind::Words: {
      size_t n = 0;
      if (!rest.empty() && rest[0] == '.')
        ++n; // directives: .org
      while (n < rest.size() && detail::is_word(rest[n]))
        ++n;
      if (n == 0)
        return 0;
      auto w = rest.substr(0, n);
      bool hit = ignorecase_ ? r.words.contains(detail::upper(w))
                             : r.words.contains(std::string{w});
      return hit ? n : 0;
    }
    case Kind::Pattern:
      return r.pattern ? r.pattern->match(s, pos) : 0;
    case Kind::Symbols:
      for (const auto &tok : r.tokens)
        if (rest.starts_with(tok))
          return tok.size();
      return 0;
    }
    return 0;
  }

  std::vector<std::string> extensions_;
  std::vector<Rule> rules_;
  bool ignorecase_ = false;
};

/// The definition in `dir` whose `extensions` lists `ext` (".fn"), or none.
/// Throws std::runtime_error naming the file when one fails to parse.
inline auto load_for(const std::filesystem::path &dir, std::string_view ext,
                     const std::vector<std::string> &isa)
    -> std::optional<Definition> {
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (entry.path().extension() != ".syntax")
      continue;
    std::ifstream in(entry.path());
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    // Cheap check before parsing every file: is the extension listed?
    if (!text.contains(ext))
      continue;
    try {
      auto d = Definition::parse(text, isa);
      if (std::ranges::contains(d.extensions(), std::string{ext}))
        return d;
    } catch (const std::exception &e) {
      throw std::runtime_error(
          std::format("{}: {}", entry.path().filename().string(), e.what()));
    }
  }
  return std::nullopt;
}

} // namespace Fanta::Syntax
