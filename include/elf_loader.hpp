#pragma once

#include <cstdint>
#include <string>

namespace riscv {

struct ElfLoadResult {
    bool ok = false;
    std::uint32_t entry_point = 0;
    std::string error;
};

class Memory;

ElfLoadResult load_elf(const std::string& path, Memory& memory);

}  // namespace riscv
