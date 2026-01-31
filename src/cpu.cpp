#include "cpu.hpp"
#include "instructions_impl.hpp"

#define EXEC_INST(OpCode, Name)\
  case OpCode: {\
    Name::exec(*this);\
    break;\
  }\

auto CPU::run_cycle() -> void {
  auto byte = fetch();
  switch(byte) {
    EXEC_INST(0x1, AddReg)
    EXEC_INST(0x2, AddImm)
    EXEC_INST(0x3, MovReg)
    EXEC_INST(0x4, MovImm)
    EXEC_INST(0x5, SubReg)
    EXEC_INST(0x6, SubImm)
  }
}
