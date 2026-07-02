#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(!sim.has_breakpoint(0x2000u));
    return 0;
}
