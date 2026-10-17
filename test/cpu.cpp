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

TEST_CASE("Flags - ADD Boundary Values") {
  using namespace Instructions;
  constexpr auto code = Program<Add<Reg<0>, Reg<1>, Reg<2>>, Halt>::load();
  {
    CPU cpu{};
    cpu.load_rom(code);
    cpu.registers[1] = 0xFFFFFFFF;
    cpu.registers[2] = 1;
    cpu.run_cycle();
    REQUIRE_SAME(0u, cpu.registers[0]);
    REQUIRE_TRUE(cpu.is_zero_set());
    REQUIRE_TRUE(cpu.is_carry_set());
    REQUIRE_TRUE(!cpu.is_overflow_set());
    REQUIRE_TRUE(!cpu.is_neg_set());
  }
  {
    CPU cpu{};
    cpu.load_rom(code);
    cpu.registers[1] = 0x7FFFFFFF; // INT32_MAX + 1 -> INT32_MIN
    cpu.registers[2] = 1;
    cpu.run_cycle();
    REQUIRE_SAME(0x80000000u, cpu.registers[0]);
    REQUIRE_TRUE(!cpu.is_zero_set());
    REQUIRE_TRUE(!cpu.is_carry_set());
    REQUIRE_TRUE(cpu.is_overflow_set());
    REQUIRE_TRUE(cpu.is_neg_set());
  }
}

TEST_CASE("Flags - SUB Boundary Values") {
  using namespace Instructions;
  constexpr auto code = Program<Sub<Reg<0>, Reg<1>, Reg<2>>, Halt>::load();
  {
    CPU cpu{};
    cpu.load_rom(code);
    cpu.registers[1] = 0;
    cpu.registers[2] = 1; // 0 - 1 borrows
    cpu.run_cycle();
    REQUIRE_SAME(0xFFFFFFFFu, cpu.registers[0]);
    REQUIRE_TRUE(cpu.is_neg_set());
    REQUIRE_TRUE(!cpu.is_carry_set()); // ARM-style: C = no borrow
    REQUIRE_TRUE(!cpu.is_overflow_set());
  }
  {
    CPU cpu{};
    cpu.load_rom(code);
    cpu.registers[1] = 0x80000000; // INT32_MIN - 1 -> INT32_MAX
    cpu.registers[2] = 1;
    cpu.run_cycle();
    REQUIRE_SAME(0x7FFFFFFFu, cpu.registers[0]);
    REQUIRE_TRUE(!cpu.is_neg_set());
    REQUIRE_TRUE(cpu.is_overflow_set());
    REQUIRE_TRUE(cpu.is_carry_set());
  }
  {
    CPU cpu{};
    cpu.load_rom(code);
    cpu.registers[1] = 5;
    cpu.registers[2] = 5;
    cpu.run_cycle();
    REQUIRE_TRUE(cpu.is_zero_set());
    REQUIRE_TRUE(cpu.is_carry_set());
  }
}

TEST_CASE("Immediates - 16-bit Payload Is Zero-Extended") {
  // Literal<> is int16_t, so 0x8000-0xFFFF payloads can't be spelled with the
  // constexpr assembler; use the runtime encoders instead.
  const std::array<uint32_t, 3> code{
      Instructions::parse_two(0x4, 0, 0xFFFF, true),      // MOV R0, #FFFF
      Instructions::parse_three(0x2, 1, 1, 0xFFFF, true), // ADD R1, R1, #FFFF
      0};
  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_cycle();
  REQUIRE_SAME(0xFFFFu, cpu.registers[0]);
  REQUIRE_TRUE(!cpu.is_neg_set());
  cpu.run_cycle();
  REQUIRE_SAME(0xFFFFu, cpu.registers[1]);
}

TEST_CASE("Signed Branches - BMI/BPL After CMP") {
  using namespace Instructions;
  constexpr auto code =
      Program<Mov<Reg<0>, Literal<3>>, Cmp<Reg<0>, Literal<5>>,
              Bmi<Target<8>>, Mov<Reg<1>, Literal<1>>, Cmp<Reg<0>, Literal<1>>,
              Bpl<Target<8>>, Mov<Reg<2>, Literal<1>>, Halt>::load();
  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_until_halt();
  REQUIRE_SAME(0, cpu.registers[1]); // BMI skipped it
  REQUIRE_SAME(0, cpu.registers[2]); // BPL skipped it
}

TEST_CASE("PC-Relative CALL/RET") {
  using namespace Instructions;
  constexpr auto code =
      Program<Call<Target<12>>,                // 0x00 -> 0x0C
              Add<Reg<2>, Reg<2>, Literal<1>>, // 0x04 return lands here
              Halt,                            // 0x08
              Mov<Reg<1>, Literal<7>>,         // 0x0C
              Ret>::load();                    // 0x10
  CPU cpu{};
  cpu.load_rom(code);
  const auto sp = cpu.get_sp();
  cpu.run_until_halt();
  REQUIRE_SAME(7, cpu.registers[1]);
  REQUIRE_SAME(1, cpu.registers[2]);
  REQUIRE_SAME(sp, cpu.get_sp());
  REQUIRE_SAME(0xC, cpu.get_pc());
}

TEST_CASE("PUSH/POP Round Trip Restores SP") {
  using namespace Instructions;
  constexpr auto code = Program<Mov<Reg<1>, Literal<42>>, Push<Target<1>>,
                                Mov<Reg<1>, Literal<0>>, Pop<Target<2>>,
                                Halt>::load();
  CPU cpu{};
  cpu.load_rom(code);
  const auto sp = cpu.get_sp();
  cpu.run_until_halt();
  REQUIRE_SAME(42, cpu.registers[2]);
  REQUIRE_SAME(sp, cpu.get_sp());
}

TEST_CASE("Load/Store With Max 16-bit Offset") {
  using namespace Instructions;
  const std::array<uint32_t, 5> code{
      Mov<Reg<0>, Literal<0x1000>>::emit(), Mov<Reg<1>, Literal<0x55>>::emit(),
      parse_three(0x8, 1, 0, 0xFFFC, true), // STORE R1, FFFC(R0)
      parse_three(0x9, 2, 0, 0xFFFC, true), // LOAD  R2, FFFC(R0)
      0};
  CPU cpu{};
  cpu.load_rom(code);
  cpu.run_until_halt();
  REQUIRE_SAME(0x55u, cpu.load(0x1000 + 0xFFFC)); // offset is unsigned
  REQUIRE_SAME(0x55u, cpu.registers[2]);
}

TEST_CASE("Stack Push Must Not Touch VRAM") {
  CPU cpu{};
  cpu.push_stack(0xAABBCCDD);
  auto *vram = cpu.get_vram();
  for (int i = 0; i < 4; i++)
    REQUIRE_SAME(0, vram[i]);
}

TEST_CASE("Initial SP Is Word-Aligned") {
  CPU cpu{};
  REQUIRE_SAME(0u, cpu.get_sp() % 4);
}

TEST_CASE("Initial SP Is The Top Word Below VRAM") {
  // The first push should land in the last word before VRAM (0x800000).
  CPU cpu{};
  const auto vramBase = static_cast<uint32_t>(cpu.get_vram() - cpu.ram.from(0));
  REQUIRE_SAME(vramBase - 4, cpu.get_sp());
}

TEST_CASE("Deep Stack Usage Stays Below VRAM") {
  // 64 nested pushes then pops: every write stays under VRAM and SP is restored.
  CPU cpu{};
  const auto sp = cpu.get_sp();
  for (uint32_t i = 0; i < 64; i++)
    cpu.push_stack(0xFFFFFFFF);
  auto *vram = cpu.get_vram();
  for (int i = 0; i < 4; i++)
    REQUIRE_SAME(0, vram[i]);
  for (int i = 0; i < 64; i++)
    REQUIRE_SAME(0xFFFFFFFFu, cpu.pop_stack());
  REQUIRE_SAME(sp, cpu.get_sp());
}
