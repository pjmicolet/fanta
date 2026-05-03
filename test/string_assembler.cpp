#include <string_assembler.hpp>
#include <testframework/testing.hpp>

TEST_CASE("Parse add") {
  Assembler assembler{};
  auto var = assembler.assemble("ADD R1, R2, R3");
  auto expected = Instructions::parse_three(0x1, 1, 2, 3, false);
  REQUIRE_SAME(expected, var);

  auto var2 = assembler.assemble("ADD R1, R2, $FF");
  auto expected2 = Instructions::parse_three(0x2, 1, 2, 0xFF, true);
  REQUIRE_SAME(expected2, var2);

  auto var3 = assembler.assemble("MOV R1, R2");
  auto expected3 = Instructions::parse_two(0x3, 1, 2, false);
  REQUIRE_SAME(expected3, var3);

  auto var4 = assembler.assemble("LOAD R1, 10(R2)");
  auto expected4 = Instructions::parse_three(0x9, 1, 2, 0x10, true);
  REQUIRE_SAME(expected4, var4);

  auto var5 = assembler.assemble("BNE -4");
  auto expected5 = Instructions::parse_one(0xB, -0x4);
  REQUIRE_SAME(expected5, var5);
}
