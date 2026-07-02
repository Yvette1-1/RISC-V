#pragma once

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

}  // namespace riscv
