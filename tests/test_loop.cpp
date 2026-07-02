#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(sim.exit_code() == 0u);
    return 0;
}
