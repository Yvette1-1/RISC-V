#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    assert(sim.state() == riscv::SimulatorState::Idle);
    assert(sim.pc() == 0u);
    return 0;
}
