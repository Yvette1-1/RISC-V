#pragma once

#include "memory.hpp"
#include "riscv_config.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

struct PipelineSnapshot {
    std::uint64_t cycle = 0;
    std::uint32_t if_pc = 0;
    std::uint32_t id_pc = 0;
    std::uint32_t ex_pc = 0;
    std::uint32_t mem_pc = 0;
    std::uint32_t wb_pc = 0;
    bool if_valid = false;
    bool id_valid = false;
    bool ex_valid = false;
    bool mem_valid = false;
    bool wb_valid = false;
};

struct PipelineStats {
    std::uint64_t cycles = 0;
    std::uint64_t instructions = 0;
    std::uint64_t stalls = 0;
    std::uint64_t flushes = 0;
    double cpi = 0.0;
};

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
    PipelineStats pipeline_stats() const noexcept;
    const PipelineSnapshot& pipeline_snapshot() const noexcept;

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
    enum class ExecuteResult { Advanced, Paused, Halted, Error, Exited };
    ExecuteResult execute_one();
    ExecuteResult handle_syscall();
    void update_pipeline(std::uint32_t retired_pc, bool control_change);

    SimulatorConfig config_;
    Memory memory_;
    SimulatorState state_ = SimulatorState::Idle;
    std::uint32_t pc_ = 0;
    std::uint32_t regs_[32]{};
    std::uint32_t fregs_[32]{};
    std::uint32_t exit_code_ = 0;
    std::uint32_t brk_ = 0;
    std::uint32_t heap_base_ = 0;
    std::uint32_t tohost_ = 0;
    std::uint32_t fromhost_ = 0;
    std::uint32_t csr_mstatus_ = 0;
    std::uint32_t csr_mtvec_ = 0;
    std::uint32_t csr_mepc_ = 0;
    std::uint32_t csr_mcause_ = 0;
    std::uint32_t csr_mtval_ = 0;
    PipelineSnapshot pipeline_{};
    PipelineStats pipeline_stats_{};
    std::vector<std::uint32_t> breakpoints_;
    std::string last_error_;
};

}  // namespace riscv
