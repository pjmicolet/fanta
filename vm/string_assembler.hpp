#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <instructions.hpp>
#include <ranges>
#include <string>
#include <vector>

struct Assembler {
  static constexpr std::string _LITERAL = "#";

  auto scan_for_labels(std::string_view code) -> void {
    labels_.clear();
    auto lines = extract_labels(code);
    uint32_t add = 0;
    for (const auto &line : lines) {
      if (is_label_define(line)) {
        size_t labelEndDef = line.find(":");
        size_t cleanLabelStart = line.find_first_not_of(" ");
        size_t end = labelEndDef - cleanLabelStart;
        std::string clean_label{line.substr(cleanLabelStart, end)};
        std::transform(clean_label.begin(), clean_label.end(),
                       clean_label.begin(), ::toupper);
        clean_label.erase(
            std::remove_if(clean_label.begin(), clean_label.end(),
                           [](unsigned char x) { return std::isspace(x); }),
            clean_label.end());
        labels_[clean_label] = add;
      }
      add += 4;
    }
  }

  auto assemble(std::string_view inst, int address) -> std::uint32_t {
    if (is_pure_comment(inst))
      return Instructions::Nop::emit();
    auto tokens = split_inst(inst);
    if (tokens.empty())
      return -1;
    if (is_label_define(inst))
      return Instructions::Nop::emit();

    try {
      auto &mtdc = Instructions::Registry::fetch(std::string{tokens[0]});
      switch (mtdc.fmt) {
      case Instructions::THREE_OP:
        return parse_three(tokens, mtdc, address);
      case Instructions::TWO_OP:
        return parse_two(tokens, mtdc, address);
      case Instructions::MEM:
        return parse_mem(tokens, mtdc, address);
      case Instructions::JUMP:
        return parse_one(tokens, mtdc, address, true);
      case Instructions::BRANCH:
        return parse_one(tokens, mtdc, address);
      case Instructions::STACK_INST:
        return parse_one(tokens, mtdc, address);
      case Instructions::HALT:
        return 0;
      case Instructions::RET:
        return 0x16 << 26;
      }
    } catch (...) {
      return -1;
    }
    return -1;
  }

  auto get_label_for(int address) -> std::string {
    for (auto const &[label, add] : labels_) {
      if (add == address)
        return label;
    }
    return "";
  }

  auto does_label_exist(std::string label) -> bool {
    return labels_.contains(label);
  }

private:
  std::string last_copy; // Store the modified string to keep string_views valid

  auto is_label_define(std::string_view line) -> bool {
    auto code = line.substr(0, line.find(';'));
    if (code.empty() || code[0] == ';')
      return false;
    return code.contains(':');
  }

  auto is_pure_comment(std::string_view line) -> bool { return line[0] == ';'; }

  auto is_label(std::string_view token) -> bool {
    if (!::isalpha(token[0]))
      return false;
    return std::all_of(
        token.data(), token.data() + token.size(),
        [](unsigned char c) { return ::isalnum(c) || c == '_'; });
  }

  auto is_register(std::string_view token) -> bool {
    if (token == "SP")
      return true;
    if (token.size() < 2)
      return false;
    if (token[0] != 'R' && token[0] != 'r')
      return false;
    return std::all_of(token.data() + 1, token.data() + token.size(),
                       [](unsigned char c) { return ::isdigit(c); });
  }

  auto extract_labels(std::string_view code) -> std::vector<std::string> {
    return code | std::views::split('\n') | std::views::transform([](auto t) {
             return std::string{std::string_view{t}};
           }) |
           std::ranges::to<std::vector>();
  }

  auto split_inst(std::string_view text) -> std::vector<std::string_view> {
    last_copy = std::string{text};
    for (auto &c : last_copy) {
      if (c == '(' || c == ')' || c == ',')
        c = ' ';
      else if (::isalpha(c))
        c = ::toupper(c);
    }

    return last_copy | std::views::split(' ') |
           std::views::transform([](auto t) { return std::string_view{t}; }) |
           std::views::filter(
               [](auto t) { return !t.empty() && t[0] != ';'; }) |
           std::ranges::to<std::vector>();
  }

  auto extract_val(std::string_view token, int address, bool is_jump = false)
      -> int {
    if (token.empty())
      return 0;

    if (token == "SP")
      return 16;

    // Check for register format "R1"
    if (is_register(token)) {
      int regNum = 0;
      std::from_chars(token.data() + 1, token.data() + token.size(), regNum);
      if (regNum >= 16)
        return -1; // Force assembly error for direct access
      return regNum;
    }

    if (is_label(token)) {
      if (is_jump)
        return labels_[std::string{token}];
      return labels_[std::string{token}] - address;
    }

    // Check for hex/immediate "-#12" or "$FF"
    bool isNeg = false;
    if (token[0] == '-') {
      isNeg = true;
      token.remove_prefix(1);
    }

    bool forceHex = false;
    if (token.starts_with("0X")) {
      token.remove_prefix(2);
      forceHex = true;
    } else if (token.starts_with("$")) {
      token.remove_prefix(1);
      forceHex = true;
    } else if (token.starts_with("#")) {
      token.remove_prefix(1);
      forceHex = true;
    }

    int realNum = 0;
    // Default to Hex (base 16) for all literal types per GEMINI.md
    auto [ptr, ec] =
        std::from_chars(token.data(), token.data() + token.size(), realNum, 16);

    if (ec != std::errc{} && !forceHex) {
      // Fallback to decimal ONLY if it wasn't a prefixed literal
      std::from_chars(token.data(), token.data() + token.size(), realNum, 10);
    }

    return isNeg ? -realNum : realNum;
  }

  auto parse_one(const std::vector<std::string_view> &tokens,
                 Instructions::InstMetadata &mtd, int address,
                 bool is_jump = false) -> uint32_t {
    if (tokens.size() < 2)
      return -1;
    int r1_int = extract_val(tokens[1], address, is_jump);
    return Instructions::parse_one(mtd.reg, r1_int);
  }

  auto parse_two(const std::vector<std::string_view> &tokens,
                 Instructions::InstMetadata &mtd, int address) -> uint32_t {
    if (tokens.size() < 3)
      return -1;
    int r1_int = extract_val(tokens[1], address);
    bool isImm = (tokens[2][0] != 'R' && tokens[2][0] != 'r');
    int r2_int = extract_val(tokens[2], address);
    return Instructions::parse_two(isImm ? mtd.imm : mtd.reg, r1_int, r2_int,
                                   isImm);
  }

  auto parse_three(const std::vector<std::string_view> &tokens,
                   Instructions::InstMetadata &mtd, int address) -> uint32_t {
    if (tokens.size() < 4)
      return -1;
    int r1_int = extract_val(tokens[1], address);
    int r2_int = extract_val(tokens[2], address);
    bool isImm = (tokens[3][0] != 'R' && tokens[3][0] != 'r');
    int r3_int = extract_val(tokens[3], address);
    return Instructions::parse_three(isImm ? mtd.imm : mtd.reg, r1_int, r2_int,
                                     r3_int, isImm);
  }

  auto parse_mem(const std::vector<std::string_view> &tokens,
                 Instructions::InstMetadata &mtd, int address) -> uint32_t {
    if (tokens.size() < 4)
      return -1;
    int dest_reg = extract_val(tokens[1], address);
    int offset = extract_val(tokens[2], address);
    int base_reg = extract_val(tokens[3], address);
    return Instructions::parse_three(mtd.reg, dest_reg, base_reg, offset, true);
  }

  std::unordered_map<std::string, int> labels_;
};
