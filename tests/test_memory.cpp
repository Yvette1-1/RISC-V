#include "memory.hpp"

#include <cassert>
#include <cstdint>

int main() {
    riscv::Memory memory(0x4000u);
    assert(memory.map_region({0x1000u, 0x100u, riscv::MEM_READ | riscv::MEM_WRITE, "data"}));
    assert(memory.store8(0x1000u, 0x12u));
    assert(memory.store16(0x1002u, 0x3456u));
    assert(memory.store32(0x1004u, 0x89abcdefu));

    std::uint8_t v8 = 0;
    std::uint16_t v16 = 0;
    std::uint32_t v32 = 0;
    assert(memory.load8(0x1000u, v8) && v8 == 0x12u);
    assert(memory.load16(0x1002u, v16) && v16 == 0x3456u);
    assert(memory.load32(0x1004u, v32) && v32 == 0x89abcdefu);
    assert(memory.regions().size() == 1);
    assert(memory.regions()[0].kind == riscv::MemoryRegionKind::Ram);

    assert(memory.map_region({0x2000u, 0x100u, riscv::MEM_READ | riscv::MEM_WRITE, "uart", riscv::MemoryRegionKind::Mmio}));
    const auto* mmio = memory.region_at(0x2000u);
    assert(mmio != nullptr);
    assert(mmio->kind == riscv::MemoryRegionKind::Mmio);
    assert(memory.is_mapped(0x2000u));
    assert(!memory.map_region({0x2080u, 0x100u, riscv::MEM_READ, "overlap"}));

    return 0;
}
