#include <print>
#include <string_assembler.hpp>
#include <testframework/testing.hpp>

TEST_CASE("Parse add") {
  Assembler assembler{};
  auto var = assembler.assemble("ADD R1, R2, R3", 0);
  auto expected = Instructions::parse_three(0x1, 1, 2, 3, false);
  REQUIRE_SAME(expected, var);

  auto var2 = assembler.assemble("ADD R1, R2, $FF", 0);
  auto expected2 = Instructions::parse_three(0x2, 1, 2, 0xFF, true);
  REQUIRE_SAME(expected2, var2);

  auto var3 = assembler.assemble("MOV R1, R2", 0);
  auto expected3 = Instructions::parse_two(0x3, 1, 2, false);
  REQUIRE_SAME(expected3, var3);

  auto var4 = assembler.assemble("LOAD R1, 10(R2)", 0);
  auto expected4 = Instructions::parse_three(0x9, 1, 2, 0x10, true);
  REQUIRE_SAME(expected4, var4);

  auto var5 = assembler.assemble("BNE -4", 0);
  auto expected5 = Instructions::parse_one(0xB, -0x4);
  REQUIRE_SAME(expected5, var5);
}

TEST_CASE("Labels") {
  Assembler assem{};
  std::string code =
      "START:\nADD R1, R1, #1\nLOOP:\nSUB R1, R1, #1\nBNE LOOP\nJMP START";
  assem.scan_for_labels(code);

  // START: should be at 0, but it's a label line so we might return -1 or 0
  // depending on impl Based on your code: if (is_label_define(inst)) return -1;
  // So START: line doesn't produce an instruction.
  // Actually, the TUI loop skips empty or label lines when patching memory.

  // ADD R1, R1, #1 is at 0x4 (if START: took a slot)
  // Wait, scan_for_labels increments add by 4 for every line, including labels.
  // So:
  // 0x00: START:
  // 0x04: ADD R1, R1, #1
  // 0x08: LOOP:
  // 0x0C: SUB R1, R1, #1
  // 0x10: BNE LOOP
  // 0x14: JMP START

  REQUIRE_SAME("START", assem.get_label_for(0));
  REQUIRE_SAME("LOOP", assem.get_label_for(8));

  // BNE LOOP at 0x10. LOOP is at 0x08. Offset = 8 - 0x10 = -8.
  uint32_t bne_instr = assem.assemble("BNE LOOP", 0x10);
  uint32_t expected_bne =
      Instructions::parse_one(Instructions::Registry::fetch("BNE").reg, -8);
  REQUIRE_SAME(expected_bne, bne_instr);

  // JMP START at 0x14. START is at 0x00. Absolute = 0.
  uint32_t jmp_instr = assem.assemble("JMP START", 0x14);
  uint32_t expected_jmp =
      Instructions::parse_one(Instructions::Registry::fetch("JMP").reg, 0);
  REQUIRE_SAME(expected_jmp, jmp_instr);
}

TEST_CASE("Label corner cases") {
  Assembler assem{};
  std::string code = "    START     :\nADD R1, R1, #1";
  assem.scan_for_labels(code);
  REQUIRE_SAME("START", assem.get_label_for(0));
}
