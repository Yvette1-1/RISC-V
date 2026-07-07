#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

enum MemoryPermission : std::uint32_t {
    MEM_NONE = 0,
    MEM_READ = 1u << 0,
    MEM_WRITE = 1u << 1,
    MEM_EXEC = 1u << 2,
};

struct MemoryRegion {
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint32_t permissions = MEM_READ | MEM_WRITE;
    std::string name;
};

class Memory {
public:
    explicit Memory(std::size_t size);

    std::size_t size() const noexcept;
    bool contains(std::uint32_t addr, std::size_t length = 1) const noexcept;

    bool map_region(const MemoryRegion& region);
    bool unmap_region(std::uint32_t base);
    void clear() noexcept;
    bool is_mapped(std::uint32_t addr) const;
    const MemoryRegion* region_at(std::uint32_t addr) const;
    const std::vector<MemoryRegion>& regions() const noexcept;

    bool load8(std::uint32_t addr, std::uint8_t& value) const;
    bool load16(std::uint32_t addr, std::uint16_t& value) const;
    bool load32(std::uint32_t addr, std::uint32_t& value) const;
    bool load_bytes(std::uint32_t addr, void* dst, std::size_t size) const;

    bool store8(std::uint32_t addr, std::uint8_t value);
    bool store16(std::uint32_t addr, std::uint16_t value);
    bool store32(std::uint32_t addr, std::uint32_t value);
    bool store_bytes(std::uint32_t addr, const void* src, std::size_t size);
    bool fill(std::uint32_t addr, std::uint8_t value, std::size_t size);

    std::uint8_t* data() noexcept;
    const std::uint8_t* data() const noexcept;

private:
    bool check_range(std::uint32_t addr, std::size_t size, std::uint32_t required_perms) const;

    std::vector<std::uint8_t> bytes_;
    std::vector<MemoryRegion> regions_;
};

}  // namespace riscv
