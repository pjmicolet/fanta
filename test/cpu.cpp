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

TEST_CASE("Basic Load") {
  using namespace Instructions;
  constexpr auto code = Program<
    Load<Reg<0>, Reg<0>, Literal<10>>
  >::load();

  CPU cpu{};
  cpu.store(10, 1234);
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(1234, cpu.registers[0]);
}

TEST_CASE("Basic Store") {
  using namespace Instructions;
  constexpr auto code = Program<
    Mov<Reg<0>, Literal<10>>,
    Mov<Reg<1>, Literal<20>>,
    Store<Reg<1>, Reg<0>, Literal<0>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  cpu.run_cycle();
  cpu.run_cycle();
  REQUIRE_SAME(10, cpu.load(10));
}

TEST_CASE("Basic Loop") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<0>, Literal<1>>,
    Jmp<Literal<0>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  for(int i = 0; i < 10; i++) { cpu.run_cycle(); }
  REQUIRE_SAME(5, cpu.registers[0]);
}

TEST_CASE("Basic real Loop") {
  using namespace Instructions;
  constexpr auto code = Program<
    Mov<Reg<0>, Literal<100>>,
    Add<Reg<1>, Reg<1>, Literal<1>>,
    Sub<Reg<0>, Reg<0>, Literal<1>>,
    Bne<Literal<4>>,
    Halt
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_until_halt();
  REQUIRE_SAME(100, cpu.registers[1]);
}
