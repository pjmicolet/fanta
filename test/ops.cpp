#include <testframework/testing.hpp>
#include "../common/cpu_info.hpp"
#include "assembler.hpp"
#include "cpu.hpp"


TEST_CASE("Basic Inst") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Reg<2>>>::load();

  constexpr Decode inst{code[0]};
  static_assert(inst.getOpcode() == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(inst.getS1() == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(inst.getS2() == 0x2, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(inst.getDestSrc() == 0x0, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum

  constexpr auto code2 = Program<
    Add<Reg<0>, Reg<1>, Literal<230>>>::load();
  
  constexpr Decode inst2{code2[0]};
  static_assert(inst2.getOpcode() == 0x2, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(inst2.getS1() == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(inst2.getImm() == 230, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
}

TEST_CASE("Multi Inst Assembler") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Literal<123>>,
    Mov<Reg<2>, Reg<0>>,
    Halt,
    Jmp<Target<20>>,
    Sub<Reg<2>, Reg<3>, Literal<0>>
  >::load();

  constexpr Decode s0{code[0]};
  constexpr Decode s1{code[1]};
  constexpr Decode s2{code[2]};
  constexpr Decode s3{code[3]};
  constexpr Decode s4{code[4]};
  static_assert( s0.getOpcode() == 0x2, "ADD opcode not good");
  static_assert( s1.getOpcode() == 0x3, "MOV opcode not good");
  static_assert( s2.getOpcode() == 0x0, "HALT opcode not good");
  static_assert( s3.getOpcode() == 0x7, "HALT opcode not good");
  static_assert( s4.getOpcode() == 0x6, "HALT opcode not good");
}

TEST_CASE("Load into cpu") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Literal<123>>,
    Mov<Reg<2>, Reg<0>>,
    Halt,
    Jmp<Target<20>>,
    Sub<Reg<2>, Reg<3>, Literal<0>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);

  const auto data = cpu.fetch(); 
  REQUIRE_SAME(0x2, Decode{data}.getOpcode());

  cpu.run_cycle();
}

TEST_CASE("Registry Round Trip - Every Opcode Encodes At Bit 26") {
  using namespace Instructions;
  for (const auto &entry : Registry::get()) {
    const auto &meta = Registry::fetch(entry.mnemonic);
    REQUIRE_SAME(meta.reg, entry.meta.reg);
    const auto raw = parse_three(meta.reg, 3, 4, 5, false);
    REQUIRE_SAME(meta.reg, Decode{raw}.getOpcode());
    REQUIRE_SAME(3, Decode{raw}.getDestSrc());
    REQUIRE_SAME(4, Decode{raw}.getS1());
    REQUIRE_SAME(5, Decode{raw}.getS2());
    REQUIRE_SAME(static_cast<uint32_t>(meta.reg) << 26, raw & 0xFC000000u);
    if (entry.meta.fmt == THREE_OP || entry.meta.fmt == TWO_OP) {
      const auto imm = parse_three(meta.imm, 1, 2, 0xFFFF, true);
      REQUIRE_SAME(meta.imm, Decode{imm}.getOpcode());
      REQUIRE_SAME(0xFFFF, Decode{imm}.getImm());
    }
  }
}

TEST_CASE("Registry Matches cpu_info Opcode Constants") {
  using namespace Instructions;
  namespace I = Fanta::Info::Instructions;
  REQUIRE_SAME(I::BNC, Registry::fetch("BNC").reg);
  REQUIRE_SAME(I::CIP, Registry::fetch("CIP").reg);
  REQUIRE_SAME(I::RET, Registry::fetch("RET").reg);
  REQUIRE_SAME(I::NOP, Registry::fetch("NOP").reg);
  REQUIRE_SAME(I::XOR_IMM, Registry::fetch("XOR").imm);
  REQUIRE_SAME(I::CMP_IMM, Registry::fetch("CMP").imm);
  REQUIRE_SAME(Nop::emit(), parse_one(Registry::fetch("NOP").reg, 0));
  REQUIRE_SAME(Ret::emit(), parse_one(Registry::fetch("RET").reg, 0));
  constexpr Decode bnc{Bnc<Target<-4>>::emit()};
  static_assert(bnc.getOpcode() == 0x20);
}

TEST_CASE("Unknown Mnemonic Throws From Registry") {
  bool threw = false;
  try {
    Instructions::Registry::fetch("FOO");
  } catch (const std::out_of_range &) {
    threw = true;
  }
  REQUIRE_TRUE(threw);
}
