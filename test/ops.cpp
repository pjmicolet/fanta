#include <testframework/testing.hpp>
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
  REQUIRE_SAME(0x2, data);

  cpu.run_cycle();
}
