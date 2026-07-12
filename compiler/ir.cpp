#include "ir.hpp"
#include <print>
#include <fanta_utils.hpp>

namespace Fanta {

auto getopName(const InstOp op) -> std::string {
  if (op == 0)
    return "Halt";
  if (op == 0x1)
    return "Add";
  if (op == 0x2)
    return "Add";
  if (op == 0x3)
    return "Mov";
  if (op == 0x4)
    return "Mov";
  if (op == 0x5)
    return "Sub";
  if (op == 0x6)
    return "Sub";
  if (op == 0x7)
    return "Jmp";
  if (op == 0x8)
    return "Store";
  if (op == 0x9)
    return "Load";
  if (op == 0xa)
    return "Beq";
  if (op == 0xb)
    return "Bne";
  if (op == 0xc)
    return "Lsh";
  if (op == 0xd)
    return "Lsh";
  if (op == 0xe)
    return "Cmp";
  if (op == 0xf)
    return "Cmp";
  if (op == 0x10)
    return "Bec";
  if (op == 0x11)
    return "Bmi";
  if (op == 0x12)
    return "Bpl";
  if (op == 0x13)
    return "Jrel";
  if (op == 0x14)
    return "Nop";
  if (op == 0x15)
    return "Call";
  if (op == 0x16)
    return "Ret";
  if (op == 0x17)
    return "And";
  if (op == 0x18)
    return "And";
  if (op == 0x19)
    return "Or";
  if (op == 0x1A)
    return "Or";
  if (op == 0x1B)
    return "Xor";
  if (op == 0x1C)
    return "Xor";
  if (op == 0x1D)
    return "Push";
  if (op == 0x1E)
    return "Pop";
  if (op == 0x1F)
    return "Cip";
  if (op == 0x20)
    return "Bnc";
  return "Nop";
}

auto numberOfSources(const InstOp op) -> int {
  if (op == 0)
    return 0;
  if (op == 0x1)
    return 2;
  if (op == 0x2)
    return 2;
  if (op == 0x3)
    return 1;
  if (op == 0x4)
    return 1;
  if (op == 0x5)
    return 2;
  if (op == 0x6)
    return 2;
  if (op == 0x7)
    return 1;
  if (op == 0x8)
    return 2;
  if (op == 0x9)
    return 2;
  if (op == 0xa)
    return 1;
  if (op == 0xb)
    return 1;
  if (op == 0xc)
    return 2;
  if (op == 0xd)
    return 2;
  if (op == 0xe)
    return 2;
  if (op == 0xf)
    return 2;
  if (op == 0x10)
    return 1;
  if (op == 0x11)
    return 1;
  if (op == 0x12)
    return 1;
  if (op == 0x13)
    return 1;
  if (op == 0x14)
    return 0;
  if (op == 0x15)
    return 1;
  if (op == 0x16)
    return 0;
  if (op == 0x17)
    return 2;
  if (op == 0x18)
    return 2;
  if (op == 0x19)
    return 2;
  if (op == 0x1A)
    return 2;
  if (op == 0x1B)
    return 2;
  if (op == 0x1C)
    return 2;
  if (op == 0x1D)
    return 1;
  if (op == 0x1E)
    return 1;
  if (op == 0x1F)
    return 1;
  if (op == 0x20)
    return 0;
  return 0;
}

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
