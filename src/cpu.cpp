#include "cpu.hpp"
#include "instructions_impl.hpp"

#define EXEC_INST(OpCode, Name)\
  case OpCode: {\
    Name::exec(*this, instr);\
    break;\
  }\

auto decodeOpt(uint32_t inst) {
  return (inst >> 26) & 0x3F;
}

auto CPU::run_cycle() -> void {
  auto instr = fetch();
  auto byte = decodeOpt(instr);
  switch(byte) {
    EXEC_INST(0x1, AddReg)
    EXEC_INST(0x2, AddImm)
    EXEC_INST(0x3, MovReg)
    EXEC_INST(0x4, MovImm)
    EXEC_INST(0x5, SubReg)
    EXEC_INST(0x6, SubImm)
    EXEC_INST(0x8, StoreReg)
    EXEC_INST(0x9, LoadReg)
  }
}
