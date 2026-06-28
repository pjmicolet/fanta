#include "cpu.hpp"
#include "../common/cpu_info.hpp"
#include "assembler.hpp"
#include "instructions.hpp"
#include <testframework/testing.hpp>

TEST_CASE("Basic Adds") {
  using namespace Instructions;
  constexpr auto code = Program<Add<Reg<0>, Reg<0>, Literal<122>>,
                                Add<Reg<0>, Reg<0>, Reg<0>>>::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[0]);
  cpu.run_cycle();
  REQUIRE_SAME(244, cpu.registers[0]);
}

TEST_CASE("Basic Mov") {
  using namespace Instructions;
  constexpr auto code = Program<Mov<Reg<1>, Literal<122>>, Mov<Reg<2>, Reg<1>>,
                                Mov<Reg<3>, Literal<250>>>::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[1]);
  cpu.run_cycle();
  REQUIRE_SAME(122, cpu.registers[2]);
  cpu.run_cycle();
  REQUIRE_SAME(250, cpu.registers[3]);
}

TEST_CASE("Basic Sub") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<122>>, Mov<Reg<2>, Literal<121>>,
              Sub<Reg<1>, Reg<0>, Reg<2>>,
              Sub<Reg<2>, Reg<1>, Literal<1>>>::load();

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
  constexpr auto code = Program<Load<Reg<1>, Reg<0>, Literal<10>>>::load();

  CPU cpu{};
  cpu.store(10, 1234);
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(1234, cpu.registers[1]);
}

TEST_CASE("Basic Store") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<10>>, Mov<Reg<1>, Literal<20>>,
              Store<Reg<1>, Reg<0>, Literal<0>>>::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  cpu.run_cycle();
  cpu.run_cycle();
  REQUIRE_SAME(20, cpu.load(10));
}

TEST_CASE("Basic Loop") {
  using namespace Instructions;
  constexpr auto code =
      Program<Add<Reg<0>, Reg<0>, Literal<1>>, Jmp<Target<0>>>::load();

  CPU cpu{};
  cpu.load_rom(code);
  for (int i = 0; i < 10; i++) {
    cpu.run_cycle();
  }
  REQUIRE_SAME(5, cpu.registers[0]);
}

TEST_CASE("Basic real Loop") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<100>>, Add<Reg<1>, Reg<1>, Literal<1>>,
              Sub<Reg<0>, Reg<0>, Literal<1>>, Bne<Target<-8>>, Halt>::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_until_halt();
  REQUIRE_SAME(100, cpu.registers[1]);
}

TEST_CASE("Flag Tests") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<1>>, Lsh<Reg<0>, Reg<0>, Literal<31>>,
              Lsh<Reg<0>, Reg<0>, Literal<1>>, Halt>::load();

  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_until_halt();
  REQUIRE_TRUE(cpu.is_carry_set());
}

TEST_CASE("Cooperative Interrupt CIP") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<0>>,
              Cip<Target<0>>, // Check interrupt 0 (VBLANK). If pending, jumps
                              // to INTERRUPT_BASE
              Add<Reg<0>, Reg<0>, Literal<1>>, // R0 = R0 + 1 (skipped if VBLANK
                                               // fires)
              Halt                             // Halt
              >::load();

  // Test Case 1: Interrupt not pending
  {
    CPU cpu{};
    cpu.cip_interrupts[0] = 0;
    cpu.load_rom(code);
    cpu.run_cycle(); // Mov R0, 0
    cpu.run_cycle(); // Cip (should not branch)
    cpu.run_cycle(); // Add R0, R0, 1
    REQUIRE_SAME(1, cpu.registers[0]);
  }

  // Test Case 2: Interrupt pending
  {
    CPU cpu{};
    cpu.cip_interrupts[0] = 1; // Simulate VBLANK hardware interrupt
    cpu.load_rom(code);

    // Write VBLANK ISR instructions directly to ROM at INTERRUPT_BASE (0x100)
    cpu.store(
        Fanta::Info::Cpu::INTERRUPT_BASE,
        Add<Reg<0>, Reg<0>, Literal<10>>::emit()); // 0x100: Add R0, R0, 10
    cpu.store(Fanta::Info::Cpu::INTERRUPT_BASE + 4, Ret::emit()); // 0x104: Ret

    cpu.run_cycle(); // Mov R0, 0
    cpu.run_cycle(); // Cip (should branch to 0x100)
    cpu.run_cycle(); // Add R0, R0, 10 (inside ISR at 0x100)
    REQUIRE_SAME(10, cpu.registers[0]);
    cpu.run_cycle(); // Ret (pops stack and jumps back to 0x8)
    cpu.run_cycle(); // Add R0, R0, 1
    REQUIRE_SAME(11, cpu.registers[0]);
  }
}
