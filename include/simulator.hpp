#pragma once

#include "riscv_config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace riscv {

class Memory;

class Simulator {
public:
    explicit Simulator(SimulatorConfig config);

    bool load_program(const std::string& path);
    bool reset();
    bool step();
    void run(std::size_t max_steps = 0);

    std::uint32_t pc() const noexcept;
    const std::uint32_t* regs() const noexcept;
    Memory& memory() noexcept;
    const Memory& memory() const noexcept;

private:
    SimulatorConfig config_;
    Memory memory_;
    std::uint32_t pc_ = 0;
    std::uint32_t regs_[32]{};
};

}  // namespace riscv
