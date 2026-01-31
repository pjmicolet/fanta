#include <testframework/testing.hpp>
#include "cpu.hpp"
#include "instructions.hpp"
#include "assembler.hpp"

TEST_CASE("Basic Adds") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<0>, Literal<122>>,
    Add<Reg<0>, Reg<0>, Reg<0>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[0]);
  cpu.run_cycle();
  REQUIRE_SAME(244, cpu.registers[0]);
}

TEST_CASE("Basic Mov") {
  using namespace Instructions;
  constexpr auto code = Program<
    Mov<Reg<1>, Literal<122>>,
    Mov<Reg<2>, Reg<1>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[1]);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[2]);
}

TEST_CASE("Basic Sub") {
  using namespace Instructions;
  constexpr auto code = Program<
    Mov<Reg<0>, Literal<122>>,
    Mov<Reg<2>, Literal<121>>,
    Sub<Reg<1>, Reg<0>, Reg<2>>,
    Sub<Reg<2>, Reg<1>, Literal<1>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  cpu.run_cycle();
  cpu.run_cycle();
  REQUIRE_SAME(1, cpu.registers[1]);
  cpu.run_cycle();
  REQUIRE_SAME(0, cpu.registers[2]);
}
