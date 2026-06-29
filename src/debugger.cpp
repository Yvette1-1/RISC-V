#include "debugger.hpp"
#include "simulator.hpp"

#include <iostream>
#include <string>

namespace riscv {

Debugger::Debugger(Simulator& simulator) : simulator_(simulator) {}

void Debugger::repl() {
    std::cout << "RISC-V debugger shell ready. Type 'quit' to exit.\n";
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "q") {
            break;
        }
        if (line == "help") {
            std::cout << "Commands: help, quit, step, run, regs, x\n";
            continue;
        }
        if (line == "regs") {
            std::cout << "pc=0x" << std::hex << simulator_.pc() << std::dec << '\n';
            continue;
        }
        std::cout << "Unknown command: " << line << '\n';
    }
}

}  // namespace riscv
