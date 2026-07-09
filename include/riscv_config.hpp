#pragma once

#include <cstdint>
#include <string>

namespace riscv {

enum class SimulatorState {
    Idle,
    Loaded,
    Running,
    Paused,
    Halted,
    Trapped,
    Exited,
    Error,
};

struct SimulatorConfig {
    std::uint32_t memory_size = 64u * 1024u * 1024u;
    std::uint32_t stack_top = 64u * 1024u * 1024u;
    std::uint32_t stack_size = 1024u * 1024u;
    std::uint32_t mmio_base = 0x0300'0000u;
    std::uint32_t mmio_size = 0x1000u;
    std::uint32_t uart_tx_addr = 0x0300'0000u;
    bool enable_trace = false;
    bool stop_on_ebreak = true;
    std::string program_path;
};

}  // namespace riscv
