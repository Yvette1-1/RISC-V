#include "debugger.hpp"
#include "simulator.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace riscv {

Debugger::Debugger(Simulator& simulator) : simulator_(simulator) {}

void Debugger::repl() {
    std::cout << "RISC-V debugger shell ready. Type 'help' for commands.\n";
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto result = handle_command(line);
        if (!result.message.empty()) {
            std::cout << result.message << '\n';
        }
        if (result.should_exit) {
            break;
        }
    }
}

DebugCommandResult Debugger::handle_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) {
        return {true, false, {}};
    }
    if (cmd == "quit" || cmd == "q") {
        return {true, true, {}};
    }
    if (cmd == "help" || cmd == "h") {
        print_help();
        return {true, false, {}};
    }
    if (cmd == "regs") {
        print_registers();
        return {true, false, {}};
    }
    if (cmd == "trace") {
        std::string mode;
        if (!(iss >> mode)) return {true, false, "trace: missing argument"};
        simulator_.set_trace_enabled(mode == "on");
        return {true, false, mode == "on" ? "Instruction trace: ON" : "Instruction trace: OFF"};
    }
    if (cmd == "b") {
        std::string tok;
        if (!(iss >> tok)) return {true, false, "b: missing address"};
        std::uint32_t addr = 0;
        if (!parse_address(tok, addr)) return {true, false, "b: invalid address"};
        add_breakpoint(addr);
        return {true, false, "Breakpoint set"};
    }
    if (cmd == "del") {
        std::size_t idx = 0;
        if (!(iss >> idx)) return {true, false, "del: missing index"};
        const auto& bps = simulator_.breakpoints();
        if (idx >= bps.size()) return {true, false, "del: out of range"};
        remove_breakpoint(bps[idx]);
        return {true, false, "Deleted breakpoint"};
    }
    if (cmd == "reset") {
        simulator_.reset();
        refresh_breakpoints();
        return {true, false, "Reset complete"};
    }
    return {true, false, "unknown command"};
}

void Debugger::refresh_breakpoints() {
    recent_breakpoints_.clear();
    for (const auto addr : simulator_.breakpoints()) {
        recent_breakpoints_.push_back(addr);
    }
}

void Debugger::print_help() const {
    std::cout << "Commands: help, quit, regs, trace on/off, b <addr>, del <n>, reset\n";
}

void Debugger::print_registers() const {
    std::cout << "PC = 0x" << std::hex << simulator_.pc() << std::dec << '\n';
}

void Debugger::print_memory(std::uint32_t, std::size_t) const {}
void Debugger::list_breakpoints() const {}
void Debugger::add_breakpoint(std::uint32_t addr) { simulator_.add_breakpoint(addr); refresh_breakpoints(); }
void Debugger::remove_breakpoint(std::uint32_t addr) { simulator_.remove_breakpoint(addr); refresh_breakpoints(); }

bool Debugger::parse_address(const std::string& token, std::uint32_t& value) {
    char* end = nullptr;
    const auto parsed = std::strtoul(token.c_str(), &end, 0);
    if (end == token.c_str() || *end != '\0') return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace riscv
