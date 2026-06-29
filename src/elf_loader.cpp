#include "elf_loader.hpp"
#include "memory.hpp"

namespace riscv {

ElfLoadResult load_elf(const std::string&, Memory&) {
    return {false, 0, "ELF loader not implemented yet"};
}

}  // namespace riscv
