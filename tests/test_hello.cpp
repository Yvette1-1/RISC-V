#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(sim.program_path().empty());
    return 0;
}
