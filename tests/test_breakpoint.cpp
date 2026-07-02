#include "simulator.hpp"

#include <cassert>
#include <cstdint>

int main() {
    riscv::Simulator sim({});
    assert(sim.add_breakpoint(0x1000u));
    assert(sim.has_breakpoint(0x1000u));
    assert(!sim.add_breakpoint(0x1000u));
    assert(sim.remove_breakpoint(0x1000u));
    assert(!sim.has_breakpoint(0x1000u));
    assert(!sim.remove_breakpoint(0x1000u));
    return 0;
}
