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

bool Memory::map_region(const MemoryRegion& region) {
    if (region.size == 0 || !contains(region.base, region.size)) {
        return false;
    }
    const auto end = static_cast<std::uint64_t>(region.base) + static_cast<std::uint64_t>(region.size);
    for (const auto& existing : regions_) {
        const auto existing_end = static_cast<std::uint64_t>(existing.base) + static_cast<std::uint64_t>(existing.size);
        if (!(end <= existing.base || existing_end <= region.base)) {
            return false;
        }
    }
    regions_.push_back(region);
    return true;
}

bool Memory::unmap_region(std::uint32_t base) {
    const auto it = std::remove_if(regions_.begin(), regions_.end(), [base](const MemoryRegion& region) {
        return region.base == base;
    });
    const bool removed = it != regions_.end();
    regions_.erase(it, regions_.end());
    return removed;
}

void Memory::clear() noexcept {
    std::fill(bytes_.begin(), bytes_.end(), 0);
    regions_.clear();
}

bool Memory::is_mapped(std::uint32_t addr) const {
    return region_at(addr) != nullptr;
}

const MemoryRegion* Memory::region_at(std::uint32_t addr) const {
    for (const auto& region : regions_) {
        if (addr >= region.base && addr < region.base + region.size) {
            return &region;
        }
    }
    return nullptr;
}

const std::vector<MemoryRegion>& Memory::regions() const noexcept {
    return regions_;
}

bool Memory::check_range(std::uint32_t addr, std::size_t size, std::uint32_t required_perms) const {
    if (!contains(addr, size)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    if (regions_.empty()) {
        return true;
    }
    const auto* region = region_at(addr);
    if (region == nullptr) {
        return false;
    }
    if ((region->permissions & required_perms) != required_perms) {
        return false;
    }
    const auto end = static_cast<std::uint64_t>(addr) + static_cast<std::uint64_t>(size);
    const auto region_end = static_cast<std::uint64_t>(region->base) + static_cast<std::uint64_t>(region->size);
    return end <= region_end;
}

bool Memory::load8(std::uint32_t addr, std::uint8_t& value) const {
    if (!check_range(addr, sizeof(value), MEM_READ)) {
        return false;
    }
    value = bytes_[addr];
    return true;
}

bool Memory::load16(std::uint32_t addr, std::uint16_t& value) const {
    if (!check_range(addr, sizeof(value), MEM_READ)) {
        return false;
    }
    value = static_cast<std::uint16_t>(bytes_[addr]) |
            static_cast<std::uint16_t>(bytes_[addr + 1]) << 8;
    return true;
}

bool Memory::load32(std::uint32_t addr, std::uint32_t& value) const {
    if (!check_range(addr, sizeof(value), MEM_READ)) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes_[addr]) |
            static_cast<std::uint32_t>(bytes_[addr + 1]) << 8 |
            static_cast<std::uint32_t>(bytes_[addr + 2]) << 16 |
            static_cast<std::uint32_t>(bytes_[addr + 3]) << 24;
    return true;
}

bool Memory::load_bytes(std::uint32_t addr, void* dst, std::size_t size) const {
    if (!check_range(addr, size, MEM_READ)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    std::memcpy(dst, bytes_.data() + addr, size);
    return true;
}

bool Memory::store8(std::uint32_t addr, std::uint8_t value) {
    if (!check_range(addr, sizeof(value), MEM_WRITE)) {
        return false;
    }
    bytes_[addr] = value;
    return true;
}

bool Memory::store16(std::uint32_t addr, std::uint16_t value) {
    if (!check_range(addr, sizeof(value), MEM_WRITE)) {
        return false;
    }
    bytes_[addr] = static_cast<std::uint8_t>(value & 0xffu);
    bytes_[addr + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    return true;
}

bool Memory::store32(std::uint32_t addr, std::uint32_t value) {
    if (!check_range(addr, sizeof(value), MEM_WRITE)) {
        return false;
    }
    bytes_[addr] = static_cast<std::uint8_t>(value & 0xffu);
    bytes_[addr + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
    bytes_[addr + 2] = static_cast<std::uint8_t>((value >> 16) & 0xffu);
    bytes_[addr + 3] = static_cast<std::uint8_t>((value >> 24) & 0xffu);
    return true;
}

bool Memory::store_bytes(std::uint32_t addr, const void* src, std::size_t size) {
    if (!check_range(addr, size, MEM_WRITE)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    std::memcpy(bytes_.data() + addr, src, size);
    return true;
}

bool Memory::fill(std::uint32_t addr, std::uint8_t value, std::size_t size) {
    if (!check_range(addr, size, MEM_WRITE)) {
        return false;
    }
    std::fill(bytes_.begin() + addr, bytes_.begin() + addr + static_cast<std::ptrdiff_t>(size), value);
    return true;
}

std::uint8_t* Memory::data() noexcept {
    return bytes_.data();
}

const std::uint8_t* Memory::data() const noexcept {
    return bytes_.data();
}

}  // namespace riscv
