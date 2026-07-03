#include "debugger.hpp"
#include "riscv_config.hpp"
#include "simulator.hpp"

#include <iostream>
#include <limits>
#include <string>

namespace {

void print_menu() {
    std::cout << "\nRISC-V Simulator Menu\n";
    std::cout << "1. Load ELF program\n";
    std::cout << "2. Enter debugger shell\n";
    std::cout << "3. Show current program path\n";
    std::cout << "4. Quit\n";
    std::cout << "Select: ";
}

std::string prompt_path() {
    std::cout << "Enter ELF path: ";
    std::string path;
    std::getline(std::cin, path);
    return path;
}

bool load_program_with_feedback(riscv::Simulator& simulator, const std::string& path) {
    if (simulator.load_program(path)) {
        std::cout << "Loaded: " << path << '\n';
        return true;
    }
    std::cout << "Failed to load program: " << path << '\n';
    std::cout << "Reason: " << simulator.last_error() << '\n';
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    riscv::SimulatorConfig config;
    riscv::Simulator simulator(config);

    if (argc > 1) {
        load_program_with_feedback(simulator, argv[1]);
    }

    riscv::Debugger debugger(simulator);

    while (true) {
        print_menu();
        int choice = 0;
        if (!(std::cin >> choice)) {
            break;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 1) {
            const auto path = prompt_path();
            if (path.empty()) {
                std::cout << "No path entered.\n";
                continue;
            }
            load_program_with_feedback(simulator, path);
        } else if (choice == 2) {
            debugger.repl();
        } else if (choice == 3) {
            const auto& path = simulator.program_path();
            if (path.empty()) {
                std::cout << "No program loaded.\n";
            } else {
                std::cout << "Current program: " << path << '\n';
            }
        } else if (choice == 4) {
            break;
        } else {
            std::cout << "Unknown option.\n";
        }
    }

    return 0;
}
