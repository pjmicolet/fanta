#pragma once
#include <cstdint>
#include <string>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <vector>
#include "instructions.hpp"

namespace Fanta {

inline auto getRegisterName(uint8_t reg) -> std::string {
    if (reg == 15) return "FP";
    if (reg == 16) return "SP";
    return "R" + std::to_string(reg);
}

inline auto disassemble(uint32_t raw, uint32_t addr = 0) -> std::string {
    Instructions::Decode d(raw);
    uint8_t opcode = d.getOpcode();

    // Mapping of opcode to name for display
    static std::unordered_map<uint8_t, std::string> names;
    if (names.empty()) {
        for (auto& [mnem, meta] : Instructions::Registry::get()) {
            names[meta.reg] = mnem;
            if (meta.imm != 0) names[meta.imm] = mnem;
        }
        names[0] = "HALT";
        names[0x14] = "NOP";
        names[0x15] = "CALL";
        names[0x16] = "RET";
    }

    if (names.find(opcode) == names.end()) {
        std::stringstream ss;
        ss << "UNKNOWN (0x" << std::hex << (int)opcode << ")";
        return ss.str();
    }
    
    std::string name = names[opcode];
    
    // Custom handling for HALT, NOP, RET
    if (opcode == 0x0) return "HALT";
    if (opcode == 0x14) return "NOP";
    if (opcode == 0x16) return "RET";

    auto& mtd = Instructions::Registry::fetch(name);
    
    std::stringstream ss;
    ss << name << " ";
    switch(mtd.fmt) {
        case Instructions::THREE_OP:
            ss << getRegisterName(d.getDestSrc()) << ", " << getRegisterName(d.getS1()) << ", ";
            if (opcode == mtd.imm) {
                ss << "#0x" << std::hex << (int)d.getImm();
            } else {
                ss << getRegisterName(d.getS2());
            }
            break;
        case Instructions::TWO_OP:
            ss << getRegisterName(d.getDestSrc()) << ", ";
            if (opcode == mtd.imm) {
                ss << "#0x" << std::hex << (int)d.getImm();
            } else {
                ss << getRegisterName(d.getS1());
            }
            break;
        case Instructions::MEM:
            ss << getRegisterName(d.getDestSrc()) << ", 0x" << std::hex << (int)d.getImm() << "(" << getRegisterName(d.getS1()) << ")";
            break;
        case Instructions::STACK_INST:
            ss << getRegisterName(raw & 0x3FFFFFF);
            break;
        case Instructions::JUMP:
        case Instructions::BRANCH: {
            if (name == "CIP") {
                ss << "#0x" << std::hex << (raw & 0x3FFFFFF);
                break;
            }
            int32_t offset = (int32_t)(raw & 0x3FFFFFF);
            // Sign extend 26-bit to 32-bit
            if (offset & 0x2000000) offset |= 0xFC000000;
            
            // For JMP (absolute) vs Branch (relative)
            uint32_t target = (opcode == Instructions::Registry::fetch("JMP").reg) ? (raw & 0x3FFFFFF) : (addr + offset);
            
            ss << "0x" << std::hex << target;
            if (opcode != Instructions::Registry::fetch("JMP").reg) {
                ss << " (offset: " << std::dec << offset << ")";
            }
            break;
        }
        default:
            ss << "UNHANDLED_FORMAT";
            break;
    }
    return ss.str();
}

inline auto disassembleProgram(const std::vector<uint32_t>& insts) -> std::string {
    std::stringstream ss;
    for (size_t i = 0; i < insts.size(); ++i) {
        ss << std::setw(4) << std::setfill('0') << std::hex << (i * 4) << ":  "
           << std::setw(8) << std::setfill('0') << insts[i] << "    "
           << disassemble(insts[i], i * 4) << "\n";
    }
    return ss.str();
}

} // namespace Fanta
