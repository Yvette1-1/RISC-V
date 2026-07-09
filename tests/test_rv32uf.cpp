#include "simulator.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::filesystem::path> collect_tests(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(dir)) {
        return files;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind("rv32uf-p-", 0) == 0 && !entry.path().has_extension()) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

bool run_one(const std::filesystem::path& elf) {
    riscv::Simulator sim({});
    if (!sim.load_program(elf.string())) {
        std::cerr << elf.filename().string() << ": load failed: " << sim.last_error() << '\n';
        return false;
    }

    sim.run();
    const auto status = sim.status();
    if (status.state != riscv::SimulatorState::Exited && status.state != riscv::SimulatorState::Halted) {
        std::cerr << elf.filename().string() << ": did not exit cleanly, state=" << static_cast<int>(status.state)
                  << " pc=0x" << std::hex << status.pc << std::dec
                  << " msg=" << status.message << '\n';
        return false;
    }

    if (status.state == riscv::SimulatorState::Exited && status.exit_code != 0u) {
        std::cerr << elf.filename().string() << ": exit code " << status.exit_code << '\n';
        return false;
    }

    return true;
}

}  // namespace

int main() {
    const auto dir = std::filesystem::path(RV32UF_TEST_DIR);
    const auto files = collect_tests(dir);
    if (files.empty()) {
        std::cerr << "No rv32uf-p-* binaries found in " << dir.string() << '\n';
        return 1;
    }

    bool ok = true;
    for (const auto& elf : files) {
        ok &= run_one(elf);
    }

    return ok ? 0 : 1;
}
