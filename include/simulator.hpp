#pragma once

#include "memory.hpp"
#include "riscv_config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

struct SimulatorStatus {
    SimulatorState state = SimulatorState::Idle;
    std::uint32_t pc = 0;
    std::uint32_t exit_code = 0;
    std::string message;
};

class Simulator {
public:
    explicit Simulator(SimulatorConfig config);

    bool load_program(const std::string& path);
    bool reset();
    bool step();
    void run(std::size_t max_steps = 0);

    SimulatorState state() const noexcept;
    SimulatorStatus status() const;
    std::uint32_t pc() const noexcept;
    std::uint32_t exit_code() const noexcept;
    std::uint32_t brk() const noexcept;
    const std::uint32_t* regs() const noexcept;
    std::uint32_t reg(std::size_t index) const;
    bool set_reg(std::size_t index, std::uint32_t value);

    Memory& memory() noexcept;
    const Memory& memory() const noexcept;

    void set_trace_enabled(bool enabled);
    bool trace_enabled() const noexcept;
    bool add_breakpoint(std::uint32_t addr);
    bool remove_breakpoint(std::uint32_t addr);
    bool has_breakpoint(std::uint32_t addr) const;
    const std::vector<std::uint32_t>& breakpoints() const noexcept;

    const std::string& program_path() const noexcept;
    const std::string& last_error() const noexcept;

private:
    enum class ExecuteResult {
        Advanced,
        Paused,
        Halted,
        Error,
        Exited,
    };

    ExecuteResult execute_one();
    ExecuteResult handle_syscall();

    SimulatorConfig config_;
    Memory memory_;
    SimulatorState state_ = SimulatorState::Idle;
    std::uint32_t pc_ = 0;
    std::uint32_t regs_[32]{};
    std::uint32_t exit_code_ = 0;
    std::uint32_t brk_ = 0;
    std::uint32_t heap_base_ = 0;
    std::vector<std::uint32_t> breakpoints_;
    std::string last_error_;
};

}  // namespace riscv
