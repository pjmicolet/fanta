#include "ir.hpp"
#include <array>
#include <print>
#include <fanta_utils.hpp>

namespace Fanta {

namespace {

struct OpInfo {
  std::string_view name;
  uint8_t numSources;
};

// Indexed directly by opcode (6-bit encoding space). Unlisted opcodes default
// to {"Nop", 0} as an explicit "unrecognized opcode" sentinel. numSources is
// an IR pretty-printing concept, not a hardware encoding one -- it's the
// count of populated *source* operands, distinct from Instructions::InstFormat
// (e.g. Cmp is TWO_OP format like Mov, but numSources=2 because its
// "destination" field is reused to hold the first comparison operand).
constexpr auto buildOpTable() {
  std::array<OpInfo, 64> t{};
  for (auto &e : t)
    e = {"Nop", 0};
  t[0x0] = {"Halt", 0};
  t[0x1] = {"Add", 2};
  t[0x2] = {"Add", 2};
  t[0x3] = {"Mov", 1};
  t[0x4] = {"Mov", 1};
  t[0x5] = {"Sub", 2};
  t[0x6] = {"Sub", 2};
  t[0x7] = {"Jmp", 1};
  t[0x8] = {"Store", 2};
  t[0x9] = {"Load", 2};
  t[0xA] = {"Beq", 1};
  t[0xB] = {"Bne", 1};
  t[0xC] = {"Lsh", 2};
  t[0xD] = {"Lsh", 2};
  t[0xE] = {"Cmp", 2};
  t[0xF] = {"Cmp", 2};
  t[0x10] = {"Bec", 1};
  t[0x11] = {"Bmi", 1};
  t[0x12] = {"Bpl", 1};
  t[0x13] = {"Jrel", 1};
  t[0x14] = {"Nop", 0};
  t[0x15] = {"Call", 1};
  t[0x16] = {"Ret", 0};
  t[0x17] = {"And", 2};
  t[0x18] = {"And", 2};
  t[0x19] = {"Or", 2};
  t[0x1A] = {"Or", 2};
  t[0x1B] = {"Xor", 2};
  t[0x1C] = {"Xor", 2};
  t[0x1D] = {"Push", 1};
  t[0x1E] = {"Pop", 1};
  t[0x1F] = {"Cip", 1};
  t[0x20] = {"Bnc", 1};
  return t;
}

inline constexpr auto kOpTable = buildOpTable();

} // namespace

auto getopName(const InstOp op) -> std::string {
  return std::string{kOpTable[op].name};
}

auto numberOfSources(const InstOp op) -> int { return kOpTable[op].numSources; }

auto printIR(const IR &irlisting) -> void {

  for (const auto &func : irlisting.functions) {
    std::println("{}:", func.name);
    for (const auto &ir : func.insts) {
      std::visit(
          overloaded{
              [&](const IROp &irop) {
                auto dc = irop.destination.isVirtual ? "v_" : "R";
                switch (numberOfSources(irop.opcode)) {
                case 2: {
                  auto s1c = irop.source1.isVirtual ? "v_" : "R";
                  auto s2c = irop.s2type == Register
                                 ? irop.source2.isVirtual ? "v_" : "R"
                                 : "#";
                  std::println("    {}{} = {} {}{} {}{}", dc,
                               irop.destination.val, getopName(irop.opcode),
                               s1c, irop.source1.val, s2c, irop.source2.val);
                  break;
                }
                case 1: {
                  auto s2c = irop.s2type == Register
                                 ? irop.source2.isVirtual ? "v_" : "R"
                                 : "#";
                  std::println("    {}{} = {} {}{}", dc, irop.destination.val,
                               getopName(irop.opcode), s2c, irop.source2.val);
                  break;
                }
                case 0:
                  std::println("    {}", getopName(irop.opcode));
                }
              },
              [&](const CallFunc &cfunc) {
                if (cfunc.dest) {
                  std::println("    {}{} = CALL {}",
                               cfunc.dest->isVirtual ? "v_" : "R",
                               cfunc.dest->val, cfunc.name);
                } else {
                  std::println("    CALL {}", cfunc.name);
                }
              },
              [&](const IRLabel &label) {
                std::println("LABEL={}", label.name);
              },
              [&](const Branch &branch) {
                std::println("    {} {}", getopName(branch.opcode),
                             branch.label);
              },
              [&](const Return &ret) { std::println("    RET"); },
              [&](const LocalGlobalBase &base) {},
          },
          ir);
    }
  }
}

} // namespace Fanta
