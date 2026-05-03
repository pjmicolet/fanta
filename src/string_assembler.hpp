#pragma once

#include <cctype>
#include <string>
#include <charconv>
#include <instructions.hpp>
#include <ranges>
#include <vector>

struct Assembler {
  static constexpr std::string _LITERAL = "#";

  auto assemble(std::string_view inst) -> std::uint32_t {
    auto tokens = split_inst(inst);
    if (tokens.empty()) return -1;

    try {
      auto& mtdc = Instructions::Registry::fetch(std::string{tokens[0]});
      switch(mtdc.fmt) {
        case Instructions::THREE_OP: return parse_three(tokens, mtdc);
        case Instructions::TWO_OP:   return parse_two(tokens, mtdc);
        case Instructions::MEM:      return parse_mem(tokens, mtdc);
        case Instructions::JUMP:     return parse_one(tokens, mtdc);
        case Instructions::BRANCH:   return parse_one(tokens, mtdc);
        case Instructions::HALT:     return 0;
      }
    } catch (...) { return -1; }
    return -1;
  }

private:
  std::string last_copy; // Store the modified string to keep string_views valid

  auto split_inst(std::string_view text) -> std::vector<std::string_view> {
    last_copy = std::string{text};
    for(auto& c : last_copy) {
      if( c == '(' || c == ')' || c == ',') c = ' ';
      else if( ::isalpha(c)) c = ::toupper(c);
    }

    return last_copy | std::views::split(' ') | 
      std::views::transform([](auto t) { return std::string_view{t}; }) | 
      std::views::filter([](auto t) { return !t.empty(); }) |
      std::ranges::to<std::vector>();
  }

  auto extract_val(std::string_view token) -> int {
    if (token.empty()) return 0;
    
    // Check for register format "R1"
    if (token[0] == 'R' || token[0] == 'r') {
        int regNum = 0;
        std::from_chars(token.data() + 1, token.data() + token.size(), regNum);
        return regNum;
    }

    // Check for hex/immediate "-0x4" or "10"
    bool isNeg = false;
    if(token[0] == '-') {
      isNeg = true;
      token.remove_prefix(1);
    }
    
    if (token.starts_with("0X")) {
        token.remove_prefix(2);
    }

    int realNum = 0;
    // Try hex first, if it fails or isn't 0x, from_chars handles it
    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), realNum, 16);
    if (ec != std::errc{}) {
        // Fallback to decimal if hex parsing failed
        std::from_chars(token.data(), token.data() + token.size(), realNum, 10);
    }
    
    return isNeg ? -realNum : realNum;
  }

  auto parse_one(const std::vector<std::string_view>& tokens, Instructions::InstMetadata& mtd) -> uint32_t {
    if (tokens.size() < 2) return -1;
    int r1_int = extract_val(tokens[1]);
    return Instructions::parse_one(mtd.reg, r1_int);
  }

  auto parse_two(const std::vector<std::string_view>& tokens, Instructions::InstMetadata& mtd) -> uint32_t {
    if (tokens.size() < 3) return -1;
    int r1_int = extract_val(tokens[1]);
    bool isImm = (tokens[2][0] != 'R' && tokens[2][0] != 'r');
    int r2_int = extract_val(tokens[2]);
    return Instructions::parse_two(isImm ? mtd.imm : mtd.reg, r1_int, r2_int, isImm);
  }

  auto parse_three(const std::vector<std::string_view>& tokens, Instructions::InstMetadata& mtd) -> uint32_t {
    if (tokens.size() < 4) return -1;
    int r1_int = extract_val(tokens[1]);
    int r2_int = extract_val(tokens[2]);
    bool isImm = (tokens[3][0] != 'R' && tokens[3][0] != 'r');
    int r3_int = extract_val(tokens[3]);
    return Instructions::parse_three(isImm ? mtd.imm : mtd.reg, r1_int, r2_int, r3_int, isImm);
  }

  auto parse_mem(const std::vector<std::string_view>& tokens, Instructions::InstMetadata& mtd) -> uint32_t {
    if (tokens.size() < 4) return -1;
    int dest_reg = extract_val(tokens[1]);
    int offset   = extract_val(tokens[2]);
    int base_reg = extract_val(tokens[3]);
    return Instructions::parse_three(mtd.reg, dest_reg, base_reg, offset, true);
  }
};
