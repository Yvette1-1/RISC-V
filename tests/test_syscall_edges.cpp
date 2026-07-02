#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(!sim.remove_breakpoint(0x1000u));
    return 0;
}
