#include "simulator.hpp"

#include "elf_loader.hpp"
#include "memory.hpp"

#include <utility>

namespace riscv {

namespace {
constexpr std::size_t kDefaultMemorySize = 64u * 1024u * 1024u;
}

Simulator::Simulator(SimulatorConfig config)
    : config_(std::move(config)), memory_(config_.memory_size ? config_.memory_size : kDefaultMemorySize) {}

bool Simulator::load_program(const std::string& path) {
    config_.program_path = path;
    const auto result = load_elf(path, memory_);
    pc_ = result.entry_point;
    return result.ok;
}

bool Simulator::reset() {
    pc_ = 0;
    for (auto& reg : regs_) {
        reg = 0;
    }
    return true;
}

bool Simulator::step() {
    return false;
}

void Simulator::run(std::size_t) {}

std::uint32_t Simulator::pc() const noexcept {
    return pc_;
}

const std::uint32_t* Simulator::regs() const noexcept {
    return regs_;
}

Memory& Simulator::memory() noexcept {
    return memory_;
}

const Memory& Simulator::memory() const noexcept {
    return memory_;
}

}  // namespace riscv
