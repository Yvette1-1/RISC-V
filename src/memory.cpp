#include "memory.hpp"

#include <algorithm>
#include <cstring>

namespace riscv {

Memory::Memory(std::size_t size) : bytes_(size, 0) {}

std::size_t Memory::size() const noexcept {
    return bytes_.size();
}

bool Memory::contains(std::uint32_t addr, std::size_t length) const noexcept {
    if (length == 0) {
        return true;
    }
    const auto start = static_cast<std::size_t>(addr);
    return start < bytes_.size() && length <= bytes_.size() - start;
}

bool Memory::load8(std::uint32_t addr, std::uint8_t& value) const {
    if (!contains(addr)) {
        return false;
    }
    value = bytes_[addr];
    return true;
}

bool Memory::load16(std::uint32_t addr, std::uint16_t& value) const {
    if (!contains(addr, sizeof(value))) {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes_[addr]) |
            static_cast<std::uint16_t>(bytes_[addr + 1]) << 8;
    return true;
}

bool Memory::load32(std::uint32_t addr, std::uint32_t& value) const {
    if (!contains(addr, sizeof(value))) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes_[addr]) |
            static_cast<std::uint32_t>(bytes_[addr + 1]) << 8 |
            static_cast<std::uint32_t>(bytes_[addr + 2]) << 16 |
            static_cast<std::uint32_t>(bytes_[addr + 3]) << 24;
    return true;
}

bool Memory::store8(std::uint32_t addr, std::uint8_t value) {
    if (!contains(addr)) {
        return false;
    }
    bytes_[addr] = value;
    return true;
}

bool Memory::store16(std::uint32_t addr, std::uint16_t value) {
    if (!contains(addr, sizeof(value))) {
        return false;
    }
    bytes_[addr] = static_cast<std::uint8_t>(value & 0xffu);
    bytes_[addr + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    return true;
}

bool Memory::store32(std::uint32_t addr, std::uint32_t value) {
    if (!contains(addr, sizeof(value))) {
        return false;
    }
    bytes_[addr] = static_cast<std::uint8_t>(value & 0xffu);
    bytes_[addr + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    bytes_[addr + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    bytes_[addr + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
    return true;
}

bool Memory::store_bytes(std::uint32_t addr, const std::uint8_t* data, std::size_t length) {
    if (!contains(addr, length)) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    std::memcpy(bytes_.data() + addr, data, length);
    return true;
}

bool Memory::fill(std::uint32_t addr, std::uint8_t value, std::size_t length) {
    if (!contains(addr, length)) {
        return false;
    }
    std::fill(bytes_.begin() + addr, bytes_.begin() + addr + static_cast<std::ptrdiff_t>(length), value);
    return true;
}

std::uint8_t* Memory::data() noexcept {
    return bytes_.data();
}

const std::uint8_t* Memory::data() const noexcept {
    return bytes_.data();
}

}  // namespace riscv
