#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace riscv {

class Memory {
public:
    explicit Memory(std::size_t size);

    std::size_t size() const noexcept;

    bool contains(std::uint32_t addr, std::size_t length = 1) const noexcept;

    bool load8(std::uint32_t addr, std::uint8_t& value) const;
    bool load16(std::uint32_t addr, std::uint16_t& value) const;
    bool load32(std::uint32_t addr, std::uint32_t& value) const;

    bool store8(std::uint32_t addr, std::uint8_t value);
    bool store16(std::uint32_t addr, std::uint16_t value);
    bool store32(std::uint32_t addr, std::uint32_t value);
    bool store_bytes(std::uint32_t addr, const std::uint8_t* data, std::size_t length);
    bool fill(std::uint32_t addr, std::uint8_t value, std::size_t length);

    std::uint8_t* data() noexcept;
    const std::uint8_t* data() const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace riscv
