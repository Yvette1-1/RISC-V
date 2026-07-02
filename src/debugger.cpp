#include "debugger.hpp"
#include "simulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace riscv {

// ─── 辅助函数 ──────────────────────────────────────────────────

namespace {

const char* reg_abi_name(std::size_t idx) {
    static const char* abi[] = {
        "zero", "ra", "sp",  "gp",  "tp",  "t0", "t1", "t2",
        "s0",   "s1", "a0",  "a1",  "a2",  "a3", "a4", "a5",
        "a6",   "a7", "s2",  "s3",  "s4",  "s5", "s6", "s7",
        "s8",   "s9", "s10", "s11", "t3",  "t4", "t5", "t6"
    };
    return (idx < 32) ? abi[idx] : "?";
}

std::string hex_str(std::uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
    return oss.str();
}

// 解析 "addr=value" 语法
bool parse_addr_write(const std::string& token,
                      std::uint32_t& addr,
                      bool& has_write,
                      std::uint32_t& value) {
    has_write = false;
    auto eq = token.find('=');
    if (eq == std::string::npos) {
        return Debugger::parse_address(token, addr);
    }
    if (eq == 0 || eq + 1 >= token.size()) return false;
    if (!Debugger::parse_address(token.substr(0, eq), addr)) return false;
    if (!Debugger::parse_address(token.substr(eq + 1), value)) return false;
    has_write = true;
    return true;
}

}  // anonymous namespace

// ─── 构造 ──────────────────────────────────────────────────────

Debugger::Debugger(Simulator& simulator) : simulator_(simulator) {}

// ─── 主循环 ────────────────────────────────────────────────────

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

// ─── 命令分发 ──────────────────────────────────────────────────

DebugCommandResult Debugger::handle_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    if (cmd.empty()) {
        return {true, false, {}};
    }

    // ── quit / q ──
    if (cmd == "quit" || cmd == "q") {
        return {true, true, {}};
    }

    // ── help / h ──
    if (cmd == "help" || cmd == "h") {
        print_help();
        return {true, false, {}};
    }

    // ── regs ──
    if (cmd == "regs") {
        print_registers();
        return {true, false, {}};
    }

    // ── info ──
    if (cmd == "info") {
        std::string sub;
        if (!(iss >> sub)) {
            print_registers();
            return {true, false, {}};
        }
        if (sub == "r") {
            print_registers();
            return {true, false, {}};
        }
        if (sub == "b") {
            list_breakpoints();
            return {true, false, {}};
        }
        return {true, false, "info: unknown subcommand '" + sub + "' — use 'info r' or 'info b'"};
    }

    // ── step / si [N] ──
    if (cmd == "step" || cmd == "si" || cmd == "s") {
        std::size_t count = 1;
        std::string arg;
        if (iss >> arg) {
            char* end = nullptr;
            auto parsed = std::strtoul(arg.c_str(), &end, 10);
            if (end == arg.c_str() || *end != '\0' || parsed == 0) {
                return {true, false, "si: invalid count '" + arg + "' — expected positive integer"};
            }
            count = parsed;
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (!simulator_.step()) {
                std::ostringstream msg;
                msg << "si: stopped early at step " << (i + 1) << "/" << count
                    << " — " << simulator_.last_error();
                return {true, false, msg.str()};
            }
        }
        print_registers();
        return {true, false, {}};
    }

    // ── continue / c / run ──
    if (cmd == "c" || cmd == "run") {
        simulator_.run();
        const auto status = simulator_.status();
        std::string msg = "Execution stopped";
        if (status.state == SimulatorState::Paused) {
            msg += " (breakpoint hit)";
        } else if (status.state == SimulatorState::Exited) {
            msg += " (exited with code " + std::to_string(status.exit_code) + ")";
        } else if (status.state == SimulatorState::Halted) {
            msg += " (halted)";
        }
        msg += " at " + hex_str(simulator_.pc());
        print_registers();
        return {true, false, msg};
    }

    // ── x addr [count]  /  x addr=value ──
    if (cmd == "x") {
        std::string tok;
        if (!(iss >> tok)) return {true, false, "x: missing address — usage: x <addr> [count]  or  x <addr>=<value>"};

        std::uint32_t addr = 0;
        std::uint32_t value = 0;
        bool has_write = false;
        if (!parse_addr_write(tok, addr, has_write, value)) {
            return {true, false, "x: invalid address '" + tok + "' — expected hex, e.g. x 0x1000"};
        }

        if (has_write) {
            if (!simulator_.memory().store32(addr, value)) {
                return {true, false, "x: write failed at " + hex_str(addr) + " — address out of range"};
            }
            return {true, false, "Wrote " + hex_str(value) + " to " + hex_str(addr)};
        }

        std::size_t count = 16;
        std::string cnt_arg;
        if (iss >> cnt_arg) {
            char* end = nullptr;
            auto parsed = std::strtoul(cnt_arg.c_str(), &end, 10);
            if (end == cnt_arg.c_str() || *end != '\0' || parsed == 0) {
                return {true, false, "x: invalid count '" + cnt_arg + "' — expected positive integer"};
            }
            count = static_cast<std::size_t>(parsed);
        }
        print_memory(addr, count);
        return {true, false, {}};
    }

    // ── break / b addr ──
    if (cmd == "b" || cmd == "break") {
        std::string tok;
        if (!(iss >> tok)) return {true, false, "b: missing address"};
        std::uint32_t addr = 0;
        if (!parse_address(tok, addr)) return {true, false, "b: invalid address"};
        add_breakpoint(addr);
        return {true, false, "Breakpoint set"};
    }

    // ── del / delete n ──
    if (cmd == "del" || cmd == "d" || cmd == "delete") {
        std::size_t idx = 0;
        if (!(iss >> idx)) return {true, false, "del: missing index"};
        const auto& bps = simulator_.breakpoints();
        if (idx >= bps.size()) return {true, false, "del: out of range"};
        remove_breakpoint(bps[idx]);
        return {true, false, "Deleted breakpoint"};
    }

    // ── trace on / off ──
    if (cmd == "trace") {
        std::string mode;
        if (!(iss >> mode)) return {true, false, "trace: missing argument"};
        bool on = (mode == "on");
        simulator_.set_trace_enabled(on);
        return {true, false, on ? "Instruction trace: ON" : "Instruction trace: OFF"};
    }

    // ── reset ──
    if (cmd == "reset") {
        simulator_.reset();
        refresh_breakpoints();
        return {true, false, "Reset complete"};
    }

    // ── unknown ──
    return {true, false, "unknown command"};
}

// ─── 断点管理 ──────────────────────────────────────────────────

void Debugger::refresh_breakpoints() {
    recent_breakpoints_.clear();
    for (const auto addr : simulator_.breakpoints()) {
        recent_breakpoints_.push_back(addr);
    }
}

void Debugger::add_breakpoint(std::uint32_t addr) {
    simulator_.add_breakpoint(addr);
    refresh_breakpoints();
}

void Debugger::remove_breakpoint(std::uint32_t addr) {
    simulator_.remove_breakpoint(addr);
    refresh_breakpoints();
}

// ─── 输出函数 ──────────────────────────────────────────────────

void Debugger::print_help() const {
    std::cout << R"(Commands:
  help / h               Show this help
  quit / q               Exit debugger
  regs                   Show all 32 registers and PC
  info r                 Same as regs
  info b                 List all breakpoints
  si [N] / step [N]      Single-step N instructions (default 1)
  c  / run               Continue execution until breakpoint or halt
  x <addr> [count]       Examine memory (hex dump), default 16 bytes
  x <addr>=<value>       Write 32-bit value to memory address
  b <addr> / break <addr> Set breakpoint at address
  del <n> / d <n>        Delete breakpoint by index
  trace on | off         Enable/disable per-instruction trace log
  reset                  Reset simulator state

All addresses and values are in hex (0x prefix optional for hex).
Examples:
  b 0x1012c              set breakpoint at entry
  si 3                    step 3 instructions
  x 0x10000 32           dump 32 bytes from 0x10000
  x 0x20000=0xdeadbeef   write value to memory
  del 0                  remove breakpoint #0
  trace on               turn on instruction trace
)";
}

void Debugger::print_registers() const {
    const auto* regs = simulator_.regs();
    std::uint32_t pc = simulator_.pc();

    std::cout << std::hex << std::setfill('0');
    std::cout << "PC  = 0x" << std::setw(8) << pc << "\n\n";

    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 2; ++col) {
            int idx = col * 16 + row;
            std::uint32_t val = (regs != nullptr) ? regs[idx] : 0;
            std::cout << "x" << std::dec << std::setw(2) << std::setfill(' ') << idx
                      << "(" << std::left << std::setw(4) << reg_abi_name(idx) << std::right << ")"
                      << "=0x" << std::hex << std::setw(8) << std::setfill('0') << val
                      << std::setfill(' ') << "  ";
        }
        std::cout << '\n';
    }
    std::cout << std::dec;
}

void Debugger::print_memory(std::uint32_t addr, std::size_t count) const {
    if (count == 0) return;
    const auto& mem = simulator_.memory();

    std::uint32_t row_start = addr & ~0xFu;
    std::size_t rows = (count + (addr - row_start) + 15) / 16;
    if (rows == 0) rows = 1;

    std::cout << std::hex << std::setfill('0');
    for (std::size_t r = 0; r < rows; ++r) {
        std::uint32_t base = row_start + static_cast<std::uint32_t>(r * 16);
        std::cout << "0x" << std::setw(8) << base << "  ";

        for (int i = 0; i < 16; ++i) {
            std::uint32_t a = base + static_cast<std::uint32_t>(i);
            std::uint8_t byte = 0;
            if (mem.load8(a, byte)) {
                std::cout << std::setw(2) << static_cast<unsigned>(byte) << ' ';
            } else {
                std::cout << "?? ";
            }
        }

        std::cout << " |";

        for (int i = 0; i < 16; ++i) {
            std::uint32_t a = base + static_cast<std::uint32_t>(i);
            std::uint8_t byte = 0;
            if (mem.load8(a, byte) && std::isprint(byte)) {
                std::cout << static_cast<char>(byte);
            } else {
                std::cout << '.';
            }
        }

        std::cout << "|\n";
    }
    std::cout << std::dec;
}

void Debugger::list_breakpoints() const {
    const auto& bps = simulator_.breakpoints();
    if (bps.empty()) {
        std::cout << "No breakpoints.\n";
        return;
    }

    std::cout << "Breakpoints (" << bps.size() << "):\n";
    for (std::size_t i = 0; i < bps.size(); ++i) {
        std::cout << "  [" << std::dec << i << "] "
                  << hex_str(bps[i]) << '\n';
    }
}

// ─── 静态工具 ──────────────────────────────────────────────────

bool Debugger::parse_address(const std::string& token, std::uint32_t& value) {
    if (token.empty()) return false;
    char* end = nullptr;
    const auto parsed = std::strtoul(token.c_str(), &end, 0);
    if (end == token.c_str() || *end != '\0') return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace riscv
