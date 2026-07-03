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
    if (cmd == "regs" || cmd == "r") {
        print_registers();
        return {true, false, {}};
    }
    if (cmd == "step" || cmd == "s") {
        if (!simulator_.step()) {
            return {true, false, simulator_.last_error().empty() ? "Step failed" : simulator_.last_error()};
        }
        print_status();
        return {true, false, {}};
    }
    if (cmd == "continue" || cmd == "c" || cmd == "run") {
        simulator_.run();
        print_status();
        return {true, false, {}};
    }
    if (cmd == "pipeline" || cmd == "pipe" || cmd == "cpi") {
        print_pipeline();
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
    std::cout << "Commands: help, quit, regs, step, continue, pipeline, trace on/off, b <addr>, del <n>, reset\n";
}

void Debugger::print_registers() const {
    static constexpr const char* names[32] = {
        "x0/zero", "x1/ra", "x2/sp", "x3/gp", "x4/tp", "x5/t0", "x6/t1", "x7/t2",
        "x8/s0", "x9/s1", "x10/a0", "x11/a1", "x12/a2", "x13/a3", "x14/a4", "x15/a5",
        "x16/a6", "x17/a7", "x18/s2", "x19/s3", "x20/s4", "x21/s5", "x22/s6", "x23/s7",
        "x24/s8", "x25/s9", "x26/s10", "x27/s11", "x28/t3", "x29/t4", "x30/t5", "x31/t6",
    };

    const auto flags = std::cout.flags();
    const auto fill = std::cout.fill();
    std::cout << "PC = 0x" << std::hex << std::setw(8) << std::setfill('0') << simulator_.pc() << '\n';
    for (std::size_t i = 0; i < 32; ++i) {
        std::cout << std::left << std::setw(8) << std::setfill(' ') << names[i] << " = 0x" << std::right
                  << std::hex << std::setw(8) << std::setfill('0') << simulator_.reg(i);
        if ((i + 1) % 4 == 0) {
            std::cout << '\n';
        } else {
            std::cout << "  ";
        }
    }
    std::cout.flags(flags);
    std::cout.fill(fill);
}

void Debugger::print_status() const {
    const auto status = simulator_.status();
    std::cout << "PC = 0x" << std::hex << status.pc << std::dec;
    if (!status.message.empty()) {
        std::cout << " (" << status.message << ')';
    }
    if (status.state == SimulatorState::Exited) {
        std::cout << ", exited with code " << status.exit_code;
    }
    std::cout << '\n';
}

void Debugger::print_pipeline() const {
    const auto stats = simulator_.pipeline_stats();
    const auto& pipe = simulator_.pipeline_snapshot();
    std::cout << "cycles=" << stats.cycles << " instructions=" << stats.instructions << " stalls=" << stats.stalls
              << " flushes=" << stats.flushes << " CPI=" << std::fixed << std::setprecision(3) << stats.cpi << '\n';
    std::cout << std::hex;
    std::cout << "IF  " << (pipe.if_valid ? "0x" : "--") << (pipe.if_valid ? pipe.if_pc : 0) << '\n';
    std::cout << "ID  " << (pipe.id_valid ? "0x" : "--") << (pipe.id_valid ? pipe.id_pc : 0) << '\n';
    std::cout << "EX  " << (pipe.ex_valid ? "0x" : "--") << (pipe.ex_valid ? pipe.ex_pc : 0) << '\n';
    std::cout << "MEM " << (pipe.mem_valid ? "0x" : "--") << (pipe.mem_valid ? pipe.mem_pc : 0) << '\n';
    std::cout << "WB  " << (pipe.wb_valid ? "0x" : "--") << (pipe.wb_valid ? pipe.wb_pc : 0) << std::dec << '\n';
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
