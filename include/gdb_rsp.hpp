#pragma once

#include "simulator.hpp"

#include <cstdint>
#include <string>

namespace riscv {

class GdbRspSession {
public:
    explicit GdbRspSession(Simulator& simulator);

    std::string handle_packet(const std::string& payload);
    static std::string encode_packet(const std::string& payload);
    static bool decode_packet(const std::string& packet, std::string& payload);

private:
    std::string read_registers() const;
    std::string read_memory(std::uint32_t addr, std::uint32_t length) const;
    std::string write_memory(std::uint32_t addr, const std::string& hex);
    static std::string hex_byte(std::uint8_t value);
    static bool parse_hex_u32(const std::string& text, std::uint32_t& value);
    static int from_hex(char ch);

    Simulator& simulator_;
};

}  // namespace riscv
