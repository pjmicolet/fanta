#pragma once

#include <string>
#include <iostream>
#include <instructions.hpp>

struct Assembler {
  static constexpr std::string _LITERAL = "#";

  auto assemble(std::string_view inst ) -> std::uint32_t {
    for(size_t i = 0; i < inst.size(); i++) {
      if(inst[i] == ' ') {
        const std::string mnm{inst.substr(0, i)};
        auto& mtdc = Instructions::Registry::fetch(mnm);
        switch(mtdc.fmt) {
          break; case Instructions::THREE_OP: return parse_three(inst.substr(i), mtdc);
          break; case Instructions::TWO_OP: return parse_two(inst.substr(i), mtdc);
          break; case Instructions::MEM: return parse_mem(inst.substr(i), mtdc);
          break; case Instructions::JUMP: return parse_one(inst.substr(i), mtdc);
          break; case Instructions::BRANCH: return parse_one(inst.substr(i), mtdc);
          break; case Instructions::HALT: return 0;
        }
      }
    }
    return -1;
  }

private:

  auto parse_one(std::string_view rest, Instructions::InstMetadata& mtd) -> uint32_t {
    auto firstComma = rest.find(',');
    auto r1 = rest.substr(0, firstComma);
    std::string cleanR1{r1.substr(r1.find_first_not_of(" "))};
    bool isImm = cleanR1[0] != 'R';
    int r1_int = isImm ? std::stoi("0x"+cleanR1.substr(1), nullptr, 16) : std::stoi(cleanR1.substr(1));
    return Instructions::parse_one(isImm ? mtd.imm : mtd.reg, r1_int);
  }

  auto parse_two(std::string_view rest, Instructions::InstMetadata& mtd) -> uint32_t {
    auto firstComma = rest.find(',');
    auto secondComma = rest.find(',',firstComma+1);

    auto r1 = rest.substr(0, firstComma);
    auto r2 = rest.substr(firstComma+1, secondComma-firstComma-1);

    std::string cleanR1{r1.substr(r1.find_first_not_of(" "))};
    std::string cleanR2{r2.substr(r2.find_first_not_of(" "))};

    bool isImm = cleanR2[0] != 'R';
    int r1_int = std::stoi(cleanR1.substr(1));
    int r2_int = isImm ? std::stoi("0x"+cleanR2.substr(1), nullptr, 16) : std::stoi(cleanR2.substr(1));
    return Instructions::parse_two(isImm ? mtd.imm : mtd.reg, r1_int, r2_int, isImm);
  }

  auto parse_three(std::string_view rest, Instructions::InstMetadata& mtd) -> uint32_t {
    auto firstComma = rest.find(',');
    auto secondComma = rest.find(',',firstComma+1);

    auto r1 = rest.substr(0, firstComma);
    auto r2 = rest.substr(firstComma+1, secondComma-firstComma-1);
    auto r3 = rest.substr(secondComma+1);

    std::string cleanR1{r1.substr(r1.find_first_not_of(" "))};
    std::string cleanR2{r2.substr(r2.find_first_not_of(" "))};
    std::string cleanR3{r3.substr(r3.find_first_not_of(" "))};

    bool isImm = cleanR3[0] != 'R';


    int r1_int = std::stoi(cleanR1.substr(1));
    int r2_int = std::stoi(cleanR2.substr(1));
    int r3_int = isImm ? std::stoi("0x"+cleanR3.substr(1), nullptr, 16) : std::stoi(cleanR3.substr(1));
    return Instructions::parse_three(isImm ? mtd.imm : mtd.reg, r1_int, r2_int, r3_int, isImm);
  }

  //LOAD R1, 10(R2)
  //STORE R3, 10(R2)
  auto parse_mem(std::string_view rest, Instructions::InstMetadata& mtd) -> uint32_t {
    auto firstComma = rest.find(',');
    auto immEnd = rest.find('(');
    auto endOf = rest.find(')');

    auto r1 = rest.substr(0, firstComma);
    auto r2 = rest.substr(firstComma+1, immEnd-firstComma-1);
    auto r3 = rest.substr(immEnd+1, endOf-(immEnd+1));

    std::string cleanR1{r1.substr(r1.find_first_not_of(" "))};
    std::string cleanR2{r2.substr(r2.find_first_not_of(" "))};
    std::string cleanR3{r3.substr(r3.find_first_not_of(" "))};
    int r1_int = std::stoi(cleanR1.substr(1));
    int r2_int = std::stoi("0x"+cleanR2, nullptr, 16);
    int r3_int = std::stoi(cleanR3.substr(1));

    return Instructions::parse_three(mtd.reg, r1_int, r3_int, r2_int, true);
  }
};
