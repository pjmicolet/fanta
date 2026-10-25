#pragma once
#include "cpu.hpp"
#include "string_assembler.hpp"
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Loading programs into a CPU, shared by fanta-tui, fanta-trace and
 * fanta-diff.
 *
 * @details No ncurses here, so it can be unit tested (test/tui.cpp).
 */
namespace Fanta::Program {

// Initial SP set by CPU() in vm/cpu.hpp: the top word below VRAM.
inline constexpr uint32_t kStackTop = 0x7FFFFC;

struct AsmError {
  size_t line; // 0-based editor line, i.e. address / 4
  std::string text;
};

inline auto isBlankOrComment(std::string_view line) -> bool {
  auto first = line.find_first_not_of(" \t\r");
  return first == std::string_view::npos || line[first] == ';';
}

/**
 * @brief Assembles one source line per word into memory at `line * 4`.
 *
 * @details Blank lines, comments and label definitions become NOPs, so
 * addresses keep matching line numbers (the assembler's own label scan counts
 * lines the same way). A line that fails to assemble also becomes a NOP and
 * is reported, so one bad line never stops the rest of the buffer.
 *
 * @return the lines that failed, in order.
 */
inline auto assembleInto(CPU &cpu, Assembler &assem,
                         const std::vector<std::string> &lines)
    -> std::vector<AsmError> {
  std::string full;
  for (const auto &l : lines)
    full += l + "\n";
  assem.scan_for_labels(full);

  std::vector<AsmError> errors;
  for (size_t i = 0; i < lines.size(); ++i) {
    auto addr = static_cast<uint32_t>(i * 4);
    uint32_t word = Instructions::Nop::emit();
    if (!isBlankOrComment(lines[i])) {
      auto instr = static_cast<uint32_t>(-1);
      try {
        instr = assem.assemble(lines[i], static_cast<int>(addr));
      } catch (...) { // NOLINT(bugprone-empty-catch)
        // Same outcome as the assembler's own -1: reported just below.
      }
      if (instr == static_cast<uint32_t>(-1))
        errors.push_back({i, lines[i]});
      else
        word = instr;
    }
    cpu.store(addr, word);
  }
  return errors;
}

inline auto storeImage(CPU &cpu, const std::vector<uint32_t> &image) -> void {
  for (size_t i = 0; i < image.size(); ++i)
    cpu.store(static_cast<uint32_t>(i * 4), image[i]);
}

inline auto readLines(const std::string &path)
    -> std::optional<std::vector<std::string>> {
  std::ifstream in(path);
  if (!in)
    return std::nullopt;
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    // Trim trailing whitespace (and \r from CRLF files).
    line.erase(line.find_last_not_of(" \t\r\n") + 1);
    lines.push_back(line);
  }
  return lines;
}

inline auto readBinary(const std::string &path)
    -> std::optional<std::vector<uint32_t>> {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return std::nullopt;
  std::vector<uint32_t> image;
  uint32_t word = 0;
  while (in.read(reinterpret_cast<char *>(&word), sizeof(word)))
    image.push_back(word);
  return image;
}

/**
 * @brief Loads `path` into `cpu`: a `.bin` is stored as a raw image, anything
 * else is assembled as text (see assembleInto).
 *
 * @return false if the file can't be read; assembly failures go in `errors`.
 */
inline auto loadInto(CPU &cpu, const std::string &path,
                     std::vector<AsmError> &errors) -> bool {
  if (path.ends_with(".bin")) {
    auto image = readBinary(path);
    if (!image)
      return false;
    storeImage(cpu, *image);
    return true;
  }
  auto lines = readLines(path);
  if (!lines)
    return false;
  Assembler assem;
  errors = assembleInto(cpu, assem, *lines);
  return true;
}

} // namespace Fanta::Program
