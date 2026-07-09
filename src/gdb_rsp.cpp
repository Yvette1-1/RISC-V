#include "gdb_rsp.hpp"

#include <iomanip>
#include <sstream>

namespace riscv {

GdbRspSession::GdbRspSession(Simulator& simulator) : simulator_(simulator) {}

std::string GdbRspSession::handle_packet(const std::string& payload) {
    if (payload == "?") {
        return "S05";
    }
    if (payload == "qSupported") {
        return "PacketSize=4000;swbreak+;hwbreak+;vContSupported+";
    }
    if (payload == "vMustReplyEmpty") {
        return "";
    }
    if (payload == "vCont?") {
        return "vCont;c;s";
    }
    if (payload == "g") {
        return read_registers();
    }
    if (payload == "c") {
        simulator_.run();
        return simulator_.state() == SimulatorState::Exited ? "W00" : "S05";
    }
    if (payload == "s") {
        simulator_.step();
        return simulator_.state() == SimulatorState::Exited ? "W00" : "S05";
    }
    if (payload.rfind("m", 0) == 0) {
        const auto comma = payload.find(',');
        if (comma == std::string::npos) {
            return "E01";
        }
        std::uint32_t addr = 0;
        std::uint32_t length = 0;
        if (!parse_hex_u32(payload.substr(1, comma - 1), addr) || !parse_hex_u32(payload.substr(comma + 1), length)) {
            return "E01";
        }
        return read_memory(addr, length);
    }
    if (payload.rfind("M", 0) == 0) {
        const auto comma = payload.find(',');
        const auto colon = payload.find(':');
        if (comma == std::string::npos || colon == std::string::npos || colon < comma) {
            return "E01";
        }
        std::uint32_t addr = 0;
        if (!parse_hex_u32(payload.substr(1, comma - 1), addr)) {
            return "E01";
        }
        return write_memory(addr, payload.substr(colon + 1));
    }
    if (payload.rfind("Z0,", 0) == 0 || payload.rfind("z0,", 0) == 0) {
        const bool set = payload[0] == 'Z';
        const auto comma1 = payload.find(',');
        const auto comma2 = payload.find(',', comma1 + 1);
        if (comma2 == std::string::npos) {
            return "E01";
        }
        std::uint32_t addr = 0;
        if (!parse_hex_u32(payload.substr(comma1 + 1, comma2 - comma1 - 1), addr)) {
            return "E01";
        }
        const bool ok = set ? simulator_.add_breakpoint(addr) : simulator_.remove_breakpoint(addr);
        return ok ? "OK" : "E02";
    }
    if (payload == "k" || payload == "D") {
        return "OK";
    }
    return "";
}

std::string GdbRspSession::encode_packet(const std::string& payload) {
    std::uint8_t checksum = 0;
    for (const auto ch : payload) {
        checksum = static_cast<std::uint8_t>(checksum + static_cast<unsigned char>(ch));
    }
    return "$" + payload + "#" + hex_byte(checksum);
}

bool GdbRspSession::decode_packet(const std::string& packet, std::string& payload) {
    if (packet.size() < 4 || packet.front() != '$' || packet[packet.size() - 3] != '#') {
        return false;
    }
    payload = packet.substr(1, packet.size() - 4);
    std::uint8_t checksum = 0;
    for (const auto ch : payload) {
        checksum = static_cast<std::uint8_t>(checksum + static_cast<unsigned char>(ch));
    }
    const int hi = from_hex(packet[packet.size() - 2]);
    const int lo = from_hex(packet[packet.size() - 1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    return checksum == static_cast<std::uint8_t>((hi << 4) | lo);
}

std::string GdbRspSession::read_registers() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < 32; ++i) {
        const auto value = simulator_.reg(i);
        out << hex_byte(static_cast<std::uint8_t>(value & 0xffu));
        out << hex_byte(static_cast<std::uint8_t>((value >> 8) & 0xffu));
        out << hex_byte(static_cast<std::uint8_t>((value >> 16) & 0xffu));
        out << hex_byte(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    }
    const auto pc = simulator_.pc();
    out << hex_byte(static_cast<std::uint8_t>(pc & 0xffu));
    out << hex_byte(static_cast<std::uint8_t>((pc >> 8) & 0xffu));
    out << hex_byte(static_cast<std::uint8_t>((pc >> 16) & 0xffu));
    out << hex_byte(static_cast<std::uint8_t>((pc >> 24) & 0xffu));
    return out.str();
}

std::string GdbRspSession::read_memory(std::uint32_t addr, std::uint32_t length) const {
    std::ostringstream out;
    for (std::uint32_t i = 0; i < length; ++i) {
        std::uint8_t value = 0;
        if (!simulator_.memory().load8(addr + i, value)) {
            return "E02";
        }
        out << hex_byte(value);
    }
    return out.str();
}

std::string GdbRspSession::write_memory(std::uint32_t addr, const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return "E03";
    }
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = from_hex(hex[i]);
        const int lo = from_hex(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return "E03";
        }
        if (!simulator_.memory().store8(addr + static_cast<std::uint32_t>(i / 2), static_cast<std::uint8_t>((hi << 4) | lo))) {
            return "E02";
        }
    }
    return "OK";
}

std::string GdbRspSession::hex_byte(std::uint8_t value) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string out(2, '0');
    out[0] = kHex[(value >> 4) & 0xf];
    out[1] = kHex[value & 0xf];
    return out;
}

bool GdbRspSession::parse_hex_u32(const std::string& text, std::uint32_t& value) {
    value = 0;
    if (text.empty()) {
        return false;
    }
    for (const auto ch : text) {
        const int digit = from_hex(ch);
        if (digit < 0) {
            return false;
        }
        value = static_cast<std::uint32_t>((value << 4) | static_cast<std::uint32_t>(digit));
    }
    return true;
}

int GdbRspSession::from_hex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

}  // namespace riscv
