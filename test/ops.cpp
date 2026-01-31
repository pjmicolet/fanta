#include <testframework/testing.hpp>
#include "assembler.hpp"
#include "cpu.hpp"


TEST_CASE("Basic Inst") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Reg<2>>>::load();

  static_assert(code[0] == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code[1] == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code[2] == 0x2, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code[3] == 0x0, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum

  constexpr auto code2 = Program<
    Add<Reg<0>, Reg<1>, Literal<230>>>::load();
  static_assert(code2[0] == 0x2, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code2[1] == 0x1, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code2[2] == 230, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
  static_assert(code2[3] == 0x0, "ADD not parsing correctly"); // Updated to 0x1 based on current Opcode enum
}

TEST_CASE("Multi Inst Assembler") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Literal<123>>,
    Mov<Reg<2>, Reg<0>>,
    Halt,
    Jmp<Literal<2>, Literal<0>>,
    Sub<Reg<2>, Reg<3>, Literal<0>>
  >::load();

  static_assert(code[0] == 0x2, "ADD opcode not good");
  static_assert(code[4] == 0x3, "MOV opcode not good");
  static_assert(code[8] == 0x0, "HALT opcode not good");
  static_assert(code[12] == 0x7, "HALT opcode not good");
  static_assert(code[16] == 0x6, "HALT opcode not good");
}

TEST_CASE("Load into cpu") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<1>, Literal<123>>,
    Mov<Reg<2>, Reg<0>>,
    Halt,
    Jmp<Literal<2>, Literal<0>>,
    Sub<Reg<2>, Reg<3>, Literal<0>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);

  const auto data = cpu.fetch(); 
  REQUIRE_SAME(0x2, data);

  cpu.run_cycle();
}
