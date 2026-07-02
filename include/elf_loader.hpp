#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

class Memory;

struct ElfSegment {
    std::uint32_t vaddr = 0;
    std::uint32_t mem_size = 0;
    std::uint32_t file_size = 0;
    std::uint32_t permissions = 0;
    std::uint32_t offset = 0;
};

struct ElfLoadResult {
    bool ok = false;
    std::uint32_t entry_point = 0;
    std::vector<ElfSegment> segments;
    std::string error;
};

ElfLoadResult load_elf(const std::string& path, Memory& memory);
std::uint32_t get_entry_point(const ElfLoadResult& result);

}  // namespace riscv
