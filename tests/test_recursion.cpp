#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(sim.brk() == 0u);
    return 0;
}
