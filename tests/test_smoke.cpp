#include "memory.hpp"

#include <cassert>
#include <cstdint>

int main() {
    riscv::Memory memory(1024);
    assert(memory.store32(0, 0x12345678u));

    std::uint32_t value = 0;
    assert(memory.load32(0, value));
    assert(value == 0x12345678u);

    return 0;
}
