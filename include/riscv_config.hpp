#pragma once

#include <cstdint>
#include <string>

namespace riscv {

struct SimulatorConfig {
    std::uint32_t memory_size = 64u * 1024u * 1024u;
    std::string program_path;
    bool enable_trace = false;
};

}  // namespace riscv
