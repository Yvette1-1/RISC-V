#pragma once

#include <cstddef>
#include <cstdint>

namespace riscv {

struct DecodedInst {
    std::uint32_t opcode = 0;
    std::uint32_t rd = 0;
    std::uint32_t funct3 = 0;
    std::uint32_t rs1 = 0;
    std::uint32_t rs2 = 0;
    std::uint32_t funct7 = 0;
};

struct DecodeTableEntry {
    const char* mnemonic;
    std::uint32_t opcode;
    std::uint32_t funct3;
    std::uint32_t funct7;
    const char* format;
    const char* semantics;
};

inline constexpr DecodeTableEntry kDecodeTable[] = {
    {"LUI", 0x37, 0xff, 0xff, "U", "rd = imm20 << 12"},
    {"AUIPC", 0x17, 0xff, 0xff, "U", "rd = pc + (imm20 << 12)"},
    {"JAL", 0x6f, 0xff, 0xff, "J", "rd = pc + 4; pc += offset"},
    {"JALR", 0x67, 0x0, 0xff, "I", "rd = pc + 4; pc = (rs1 + imm) & ~1"},
    {"BEQ", 0x63, 0x0, 0xff, "B", "branch if rs1 == rs2"},
    {"BNE", 0x63, 0x1, 0xff, "B", "branch if rs1 != rs2"},
    {"BLT", 0x63, 0x4, 0xff, "B", "branch if signed rs1 < rs2"},
    {"BGE", 0x63, 0x5, 0xff, "B", "branch if signed rs1 >= rs2"},
    {"BLTU", 0x63, 0x6, 0xff, "B", "branch if unsigned rs1 < rs2"},
    {"BGEU", 0x63, 0x7, 0xff, "B", "branch if unsigned rs1 >= rs2"},
    {"LB", 0x03, 0x0, 0xff, "I", "rd = sign_extend(mem8[rs1 + imm])"},
    {"LH", 0x03, 0x1, 0xff, "I", "rd = sign_extend(mem16[rs1 + imm])"},
    {"LW", 0x03, 0x2, 0xff, "I", "rd = mem32[rs1 + imm]"},
    {"LBU", 0x03, 0x4, 0xff, "I", "rd = zero_extend(mem8[rs1 + imm])"},
    {"LHU", 0x03, 0x5, 0xff, "I", "rd = zero_extend(mem16[rs1 + imm])"},
    {"SB", 0x23, 0x0, 0xff, "S", "mem8[rs1 + imm] = rs2[7:0]"},
    {"SH", 0x23, 0x1, 0xff, "S", "mem16[rs1 + imm] = rs2[15:0]"},
    {"SW", 0x23, 0x2, 0xff, "S", "mem32[rs1 + imm] = rs2"},
    {"ADDI", 0x13, 0x0, 0xff, "I", "rd = rs1 + imm"},
    {"SLTI", 0x13, 0x2, 0xff, "I", "rd = signed(rs1) < imm"},
    {"SLTIU", 0x13, 0x3, 0xff, "I", "rd = rs1 < imm"},
    {"XORI", 0x13, 0x4, 0xff, "I", "rd = rs1 ^ imm"},
    {"ORI", 0x13, 0x6, 0xff, "I", "rd = rs1 | imm"},
    {"ANDI", 0x13, 0x7, 0xff, "I", "rd = rs1 & imm"},
    {"SLLI", 0x13, 0x1, 0x00, "I", "rd = rs1 << shamt"},
    {"SRLI", 0x13, 0x5, 0x00, "I", "rd = rs1 >> shamt"},
    {"SRAI", 0x13, 0x5, 0x20, "I", "rd = signed(rs1) >> shamt"},
    {"ADD", 0x33, 0x0, 0x00, "R", "rd = rs1 + rs2"},
    {"SUB", 0x33, 0x0, 0x20, "R", "rd = rs1 - rs2"},
    {"SLL", 0x33, 0x1, 0x00, "R", "rd = rs1 << rs2[4:0]"},
    {"SLT", 0x33, 0x2, 0x00, "R", "rd = signed(rs1) < signed(rs2)"},
    {"SLTU", 0x33, 0x3, 0x00, "R", "rd = rs1 < rs2"},
    {"XOR", 0x33, 0x4, 0x00, "R", "rd = rs1 ^ rs2"},
    {"SRL", 0x33, 0x5, 0x00, "R", "rd = rs1 >> rs2[4:0]"},
    {"SRA", 0x33, 0x5, 0x20, "R", "rd = signed(rs1) >> rs2[4:0]"},
    {"OR", 0x33, 0x6, 0x00, "R", "rd = rs1 | rs2"},
    {"AND", 0x33, 0x7, 0x00, "R", "rd = rs1 & rs2"},
    {"FENCE", 0x0f, 0x0, 0xff, "I", "ordering fence"},
    {"ECALL", 0x73, 0x0, 0x00, "I", "environment call"},
    {"EBREAK", 0x73, 0x0, 0x01, "I", "debug breakpoint"},
};

inline constexpr std::size_t kDecodeTableSize = sizeof(kDecodeTable) / sizeof(kDecodeTable[0]);

constexpr std::uint32_t get_bits(std::uint32_t value, std::uint32_t high, std::uint32_t low) {
    return (high < low || high >= 32) ? 0u : ((value >> low) & ((high == 31 && low == 0) ? 0xffffffffu : ((1u << (high - low + 1u)) - 1u)));
}

constexpr std::int32_t sign_extend(std::uint32_t value, std::uint32_t bits) {
    return bits == 0 ? 0 : static_cast<std::int32_t>(static_cast<std::int32_t>(value << (32u - bits)) >> (32u - bits));
}

constexpr DecodedInst decode(std::uint32_t inst) {
    return DecodedInst{
        get_bits(inst, 6, 0),
        get_bits(inst, 11, 7),
        get_bits(inst, 14, 12),
        get_bits(inst, 19, 15),
        get_bits(inst, 24, 20),
        get_bits(inst, 31, 25),
    };
}

inline constexpr const char* decode_mnemonic(const DecodedInst& decoded) {
    for (const auto& entry : kDecodeTable) {
        if (entry.opcode == decoded.opcode &&
            (entry.funct3 == 0xff || entry.funct3 == decoded.funct3) &&
            (entry.funct7 == 0xff || entry.funct7 == decoded.funct7)) {
            return entry.mnemonic;
        }
    }
    return "UNKNOWN";
}

}  // namespace riscv
