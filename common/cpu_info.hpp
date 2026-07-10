#pragma once
#include <cstdint>

namespace Fanta {
namespace Info {

namespace Instructions {
constexpr static uint32_t HALT = 0x0;
constexpr static uint32_t ADD_REG = 0x1;
constexpr static uint32_t ADD_IMM = 0x2;
constexpr static uint32_t MOV_REG = 0x3;
constexpr static uint32_t MOV_IMM = 0x4;
constexpr static uint32_t SUB_REG = 0x5;
constexpr static uint32_t SUB_IMM = 0x6;
constexpr static uint32_t JUMP = 0x7;
constexpr static uint32_t STORE = 0x8;
constexpr static uint32_t LOAD = 0x9;
constexpr static uint32_t BEQ = 0xA;
constexpr static uint32_t BNE = 0xB;
constexpr static uint32_t LSH_REG = 0xC;
constexpr static uint32_t LSH_IMM = 0xD;
constexpr static uint32_t CMP_REG = 0xE;
constexpr static uint32_t CMP_IMM = 0xF;
constexpr static uint32_t BEC = 0x10;
constexpr static uint32_t BMI = 0x11;
constexpr static uint32_t BPL = 0x12;
constexpr static uint32_t JREL = 0x13;
constexpr static uint32_t NOP = 0x14;
constexpr static uint32_t CALL = 0x15;
constexpr static uint32_t RET = 0x16;
constexpr static uint32_t AND_REG = 0x17;
constexpr static uint32_t AND_IMM = 0x18;
constexpr static uint32_t OR_REG = 0x19;
constexpr static uint32_t OR_IMM = 0x1A;
constexpr static uint32_t XOR_REG = 0x1B;
constexpr static uint32_t XOR_IMM = 0x1C;
constexpr static uint32_t PUSH = 0x1D;
constexpr static uint32_t POP = 0x1E;
constexpr static uint32_t CIP = 0x1F;
constexpr static uint32_t BNC = 0x20;
} // namespace Instructions

namespace Registers {
constexpr static uint32_t FP = 15;
constexpr static uint32_t SP = 16;
} // namespace Registers
//

namespace Cpu {
constexpr static uint32_t INTERRUPT_BASE = 0x100;
};

} // namespace Info
} // namespace Fanta
