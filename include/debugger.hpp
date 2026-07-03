#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

class Simulator;

struct DebugCommandResult {
    bool handled = false;
    bool should_exit = false;
    std::string message;
};

class Debugger {
public:
    explicit Debugger(Simulator& simulator);

    void repl();
    DebugCommandResult handle_command(const std::string& line);
    void refresh_breakpoints();

private:
    void print_help() const;
    void print_registers() const;
    void print_status() const;
    void print_pipeline() const;
    void print_memory(std::uint32_t addr, std::size_t count) const;
    void list_breakpoints() const;
    void add_breakpoint(std::uint32_t addr);
    void remove_breakpoint(std::uint32_t addr);
    static bool parse_address(const std::string& token, std::uint32_t& value);

    Simulator& simulator_;
    std::vector<std::uint32_t> recent_breakpoints_;
};

}  // namespace riscv
