#pragma once
#include "cpu_info.hpp"
#include "instructions.hpp"
#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Fanta {

/**
 * @brief Register name as the assembler spells it (R0-R15, SP).
 *
 * @details R15 is the frame pointer by convention, but the assembler has no
 * "FP" alias, so we print R15 to keep the output re-assemblable.
 */
inline auto getRegisterName(uint8_t reg) -> std::string {
  if (reg == 16)
    return "SP";
  return "R" + std::to_string(reg);
}

namespace detail {

/// Opcode -> mnemonic, built once from the Registry. Reg and imm variants of
/// an instruction share a mnemonic.
inline auto opcodeNames() -> const std::array<std::string_view, 64> & {
  static const auto names = [] {
    std::array<std::string_view, 64> n{};
    for (auto const &[mnem, meta] : Instructions::Registry::get()) {
      n[meta.reg] = mnem;
      if (meta.imm != 0)
        n[meta.imm] = mnem;
    }
    return n;
  }();
  return names;
}

/// Immediate in assembler syntax: literals are hex by default, '#' marks
/// them, and a leading '-' negates (e.g. #1F, -#8).
inline auto hexImm(int32_t v) -> std::string {
  std::stringstream ss;
  if (v < 0)
    ss << "-";
  ss << "#" << std::uppercase << std::hex << (v < 0 ? -int64_t{v} : v);
  return ss.str();
}

inline auto hexAddr(uint32_t v) -> std::string {
  std::stringstream ss;
  ss << "0x" << std::uppercase << std::hex << v;
  return ss.str();
}

} // namespace detail

/// Default label lookup: no labels known.
struct NoLabels {
  auto operator()(uint32_t /*addr*/) const -> std::optional<std::string> {
    return std::nullopt;
  }
};

/**
 * @brief Disassembles one instruction word.
 *
 * @details The output is valid input for the string assembler wherever the
 * assembler can express it: immediates are '#'-prefixed hex, memory operands
 * are `#off(Rn)`, and branches print their PC-relative offset (or the target
 * label when `labels` knows one). The absolute target of a branch goes in a
 * trailing ';' comment, which the assembler ignores.
 *
 * @param raw: instruction word.
 * @param addr: address the word lives at, used to resolve branch targets.
 * @param labels: callable `uint32_t -> std::optional<std::string>`.
 */
template <typename LabelFn = NoLabels>
inline auto disassemble(uint32_t raw, uint32_t addr = 0,
                        const LabelFn &labels = {}) -> std::string {
  Instructions::Decode d(raw);
  uint8_t opcode = d.getOpcode();

  auto name = detail::opcodeNames()[opcode];
  if (name.empty()) {
    std::stringstream ss;
    ss << "UNKNOWN (0x" << std::hex << (int)opcode << ")";
    return ss.str();
  }

  const auto &mtd = Instructions::Registry::fetch(name);
  bool isImm = mtd.imm != 0 && opcode == mtd.imm;

  std::stringstream ss;
  ss << name;
  switch (mtd.fmt) {
  case Instructions::HALT: // no operands
  case Instructions::NOP:
  case Instructions::RET:
    break;
  case Instructions::THREE_OP:
    ss << " " << getRegisterName(d.getDestSrc()) << ", "
       << getRegisterName(d.getS1()) << ", "
       << (isImm ? detail::hexImm(d.getImm()) : getRegisterName(d.getS2()));
    break;
  case Instructions::TWO_OP:
    ss << " " << getRegisterName(d.getDestSrc()) << ", "
       << (isImm ? detail::hexImm(d.getImm()) : getRegisterName(d.getS1()));
    break;
  case Instructions::MEM:
    // '#' matters here: a bare offset like FFFC would parse as a label.
    ss << " " << getRegisterName(d.getDestSrc()) << ", "
       << detail::hexImm(d.getImm()) << "(" << getRegisterName(d.getS1())
       << ")";
    break;
  case Instructions::STACK_INST:
    ss << " " << getRegisterName(static_cast<uint8_t>(raw & 0x3FFFFFF));
    break;
  case Instructions::JUMP:
  case Instructions::BRANCH: {
    uint32_t field = raw & 0x3FFFFFF;
    if (opcode == Info::Instructions::CIP) {
      ss << " " << detail::hexImm(static_cast<int32_t>(field));
      break;
    }
    // JMP is absolute; everything else (branches, JREL, CALL) is PC-relative
    // to the instruction's own address.
    bool absolute = opcode == Info::Instructions::JUMP;
    int32_t offset = static_cast<int32_t>(field << 6) >> 6; // sign-extend 26
    uint32_t target = absolute ? field : addr + offset;

    if (auto label = labels(target)) {
      ss << " " << *label << " ; " << detail::hexAddr(target);
    } else {
      ss << " " << detail::hexImm(absolute ? static_cast<int32_t>(field) : offset)
         << " ; -> " << detail::hexAddr(target);
    }
    break;
  }
  }
  return ss.str();
}

inline auto disassembleProgram(const std::vector<uint32_t> &insts)
    -> std::string {
  std::stringstream ss;
  for (size_t i = 0; i < insts.size(); ++i) {
    ss << std::setw(4) << std::setfill('0') << std::hex << (i * 4) << ":  "
       << std::setw(8) << std::setfill('0') << insts[i] << "    "
       << disassemble(insts[i], static_cast<uint32_t>(i * 4)) << "\n";
  }
  return ss.str();
}

} // namespace Fanta
