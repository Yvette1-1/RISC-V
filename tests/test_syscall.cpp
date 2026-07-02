#include "simulator.hpp"

#include <cassert>
#include <cstdint>

int main() {
    riscv::Simulator sim({});
    assert(sim.state() == riscv::SimulatorState::Idle);
    assert(sim.set_reg(1, 0x1234u));
    assert(sim.reg(1) == 0x1234u);
    return 0;
}
