#include "cpu.hpp"
#include "instructions_impl.hpp"

static constexpr uint32_t CYCLES_PER_FRAME = 50000;
uint32_t inst_count = 0;

#define EXEC_INST(OpCode, Name)                                                \
  case OpCode: {                                                               \
    Name::exec(*this, instr);                                                  \
    break;                                                                     \
  }

auto decodeOpt(uint32_t inst) { return (inst >> 26) & 0x3F; }

auto CPU::run_cycle() -> void {
  auto instr = fetch();
  auto byte = decodeOpt(instr);
  if (inst_count >= CYCLES_PER_FRAME) {
    cip_interrupts[0] = 1;
    inst_count = 0;
  }
  switch (byte) {
    EXEC_INST(0, Halt)
    EXEC_INST(0x1, AddReg)
    EXEC_INST(0x2, AddImm)
    EXEC_INST(0x3, MovReg)
    EXEC_INST(0x4, MovImm)
    EXEC_INST(0x5, SubReg)
    EXEC_INST(0x6, SubImm)
    EXEC_INST(0x7, Jmp)
    EXEC_INST(0x8, StoreReg)
    EXEC_INST(0x9, LoadReg)
    EXEC_INST(0xa, Beq)
    EXEC_INST(0xb, Bne)
    EXEC_INST(0xc, LshReg)
    EXEC_INST(0xd, LshImm)
    EXEC_INST(0xe, CmpReg)
    EXEC_INST(0xf, CmpImm)
    EXEC_INST(0x10, Bec)
    EXEC_INST(0x11, Bmi)
    EXEC_INST(0x12, Bpl)
    EXEC_INST(0x13, Jrel)
    EXEC_INST(0x14, Nop)
    EXEC_INST(0x15, Call)
    EXEC_INST(0x16, Ret)
    EXEC_INST(0x17, AndReg)
    EXEC_INST(0x18, AndImm)
    EXEC_INST(0x19, OrReg)
    EXEC_INST(0x1A, OrImm)
    EXEC_INST(0x1B, XorReg)
    EXEC_INST(0x1C, XorImm)
    EXEC_INST(0x1D, Push)
    EXEC_INST(0x1E, Pop)
    EXEC_INST(0x1F, Cip)
  }
  inst_count++;
}

auto CPU::run_until_halt() -> void {
  while (!halted) {
    run_cycle();
  }
}
