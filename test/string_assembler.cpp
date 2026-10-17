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
  std::string code2 = "    STARTWITHCODE     :ADD R1, R1, #1";
  assem.scan_for_labels(code2);
  REQUIRE_SAME("STARTWITHCODE", assem.get_label_for(0));
  std::string code3 = "    STARTCOMMENT     :ADD R1, R1, #1; "
                      "FAKELABEL:\nNICELABEL:\n;JUSTCOMMENT:\n:";
  assem.scan_for_labels(code3);
  REQUIRE_TRUE(assem.does_label_exist("STARTCOMMENT"));
  REQUIRE_TRUE(assem.does_label_exist("NICELABEL"));
  REQUIRE_TRUE(assem.does_label_exist("NICELABEL"));
  REQUIRE_TRUE(!assem.does_label_exist("JUSTCOMMENT"));
  REQUIRE_TRUE(!assem.does_label_exist("JUSTCOMMENT"));
  REQUIRE_TRUE(!assem.does_label_exist("FAKELABEL"));
}

TEST_CASE("Lowercase Mnemonics And Registers Are Normalised") {
  Assembler a{};
  REQUIRE_SAME(a.assemble("ADD R1, R2, R3", 0), a.assemble("add r1, r2, r3", 0));
  REQUIRE_SAME(a.assemble("LOAD R1, 10(R2)", 0), a.assemble("load r1, 10(r2)", 0));
  REQUIRE_SAME(a.assemble("MOV R1, #ff", 0), a.assemble("MOV R1, #FF", 0));
}

TEST_CASE("Literals Are Hex By Default") {
  Assembler a{};
  const auto expected = Instructions::parse_three(0x2, 1, 2, 0x10, true);
  REQUIRE_SAME(expected, a.assemble("ADD R1, R2, 10", 0));
  REQUIRE_SAME(expected, a.assemble("ADD R1, R2, #10", 0));
  REQUIRE_SAME(expected, a.assemble("ADD R1, R2, $10", 0));
  REQUIRE_SAME(expected, a.assemble("ADD R1, R2, 0x10", 0));
}

TEST_CASE("Immediate Bounds - 0x0000 and 0xFFFF Accepted") {
  Assembler a{};
  REQUIRE_SAME(Instructions::parse_two(0x4, 1, 0x0, true), a.assemble("MOV R1, #0", 0));
  REQUIRE_SAME(Instructions::parse_two(0x4, 1, 0xFFFF, true),
               a.assemble("MOV R1, #FFFF", 0));
}

TEST_CASE("Unknown Mnemonic And Missing Operands Fail Cleanly") {
  Assembler a{};
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("FROB R1, R2", 0));
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("ADD R1, R2", 0));
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("MOV R1", 0));
  REQUIRE_SAME(0xFFFFFFFFu, a.assemble("BNE", 0));
  REQUIRE_SAME(Instructions::Nop::emit(), a.assemble("; just a comment", 0));
}

TEST_CASE("Negative Branch Offset Round Trips Through Decode") {
  Assembler a{};
  a.scan_for_labels("LOOP:\nSUB R1, R1, #1\nBNE LOOP");
  const auto raw = a.assemble("BNE LOOP", 8);
  REQUIRE_SAME(0xB, Instructions::Decode{raw}.getOpcode());
  const auto off = static_cast<int32_t>((raw & 0x3FFFFFF) << 6) >> 6;
  REQUIRE_SAME(-8, off);
}

TEST_CASE("CALL Encodes A PC-Relative Label Offset") {
  Assembler a{};
  a.scan_for_labels("NOP\nNOP\nCALL SUB\nHALT\nSUB:\nRET");
  const auto raw = a.assemble("CALL SUB", 8);
  REQUIRE_SAME(0x15, Instructions::Decode{raw}.getOpcode());
  const auto off = static_cast<int32_t>((raw & 0x3FFFFFF) << 6) >> 6;
  REQUIRE_SAME(8, off); // SUB at 0x10, CALL at 0x08
}
