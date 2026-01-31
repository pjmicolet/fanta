#include <testframework/testing.hpp>
#include "cpu.hpp"
#include "instructions.hpp"
#include "assembler.hpp"

TEST_CASE("Basic Adds") {
  using namespace Instructions;
  constexpr auto code = Program<
    Add<Reg<0>, Reg<0>, Literal<123>>
  >::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(123, cpu.registers[0]);
}

