#include "debugger.hpp"
#include "riscv_config.hpp"
#include "simulator.hpp"

#include <iostream>

int main(int argc, char** argv) {
    riscv::SimulatorConfig config;
    riscv::Simulator simulator(config);

    if (argc > 1) {
        if (!simulator.load_program(argv[1])) {
            std::cerr << "Failed to load program: " << argv[1] << '\n';
        }
    }

    riscv::Debugger debugger(simulator);
    debugger.repl();
    return 0;
}
