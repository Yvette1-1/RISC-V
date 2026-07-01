#include "debugger.hpp"
#include "memory.hpp"
#include "simulator.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace riscv {

namespace {

// ─── 日志函数 ──────────────────────────────────────────────────

void log_trace(const std::string& msg) {
    std::cerr << "[trace] " << msg << '\n';
}

void log_info(const std::string& msg) {
    std::cerr << "[info]  " << msg << '\n';
}

void log_error(const std::string& msg) {
    std::cerr << "[error] " << msg << '\n';
}

// ─── 字符串辅助 ────────────────────────────────────────────────

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }
    return tokens;
}

bool parse_hex32(const std::string& s, std::uint32_t& out) {
    if (s.empty()) return false;
    const char* p = s.c_str();
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return sscanf(p + 2, "%x", &out) == 1;
    }
    return sscanf(p, "%x", &out) == 1;
}

bool parse_dec(const std::string& s, std::size_t& out) {
    if (s.empty()) return false;
    return sscanf(s.c_str(), "%zu", &out) == 1;
}

// 解析可能带 = 的参数：x 0x1000=0xdeadbeef → addr, has_write, value
bool parse_addr_arg(const std::string& s,
                    std::uint32_t& addr,
                    bool& has_write,
                    std::uint32_t& value) {
    has_write = false;
    auto eq = s.find('=');
    if (eq == std::string::npos) {
        return parse_hex32(s, addr);
    }
    // 有 = 号：addr=value
    if (eq == 0 || eq + 1 >= s.size()) return false;
    if (!parse_hex32(s.substr(0, eq), addr)) return false;
    if (!parse_hex32(s.substr(eq + 1), value)) return false;
    has_write = true;
    return true;
}

// ─── 寄存器 ABI 名称 ───────────────────────────────────────────

const char* reg_abi_name(std::size_t idx) {
    static const char* abi[] = {
        "zero", "ra", "sp",  "gp",  "tp",  "t0", "t1", "t2",
        "s0",   "s1", "a0",  "a1",  "a2",  "a3", "a4", "a5",
        "a6",   "a7", "s2",  "s3",  "s4",  "s5", "s6", "s7",
        "s8",   "s9", "s10", "s11", "t3",  "t4", "t5", "t6"
    };
    return (idx < 32) ? abi[idx] : "?";
}

// ─── hex 字符串格式化 ──────────────────────────────────────────

std::string hex_str(std::uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
    return oss.str();
}

}  // unnamed namespace

// ═══════════════════════════════════════════════════════════════
// 公有接口
// ═══════════════════════════════════════════════════════════════

Debugger::Debugger(Simulator& simulator) : simulator_(simulator) {}

bool Debugger::has_breakpoint(std::uint32_t pc) const {
    for (const auto& bp : breakpoints_) {
        if (bp.enabled && bp.addr == pc) {
            return true;
        }
    }
    return false;
}

void Debugger::repl() {
    std::cout << "RISC-V Debugger Shell\n"
              << "Type 'help' for commands, 'quit' to exit.\n\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        // 去除首尾空白
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty()) continue;

        auto tokens = tokenize(line);
        if (tokens.empty()) continue;

        const auto& cmd = tokens[0];

        // ── quit / q ──
        if (cmd == "quit" || cmd == "q") {
            break;
        }
        // ── help / h ──
        else if (cmd == "help" || cmd == "h") {
            print_help();
        }
        // ── info ──
        else if (cmd == "info") {
            if (tokens.size() >= 2 && tokens[1] == "b") {
                cmd_breakpoint_list();
            } else {
                print_regs();  // info / info r
            }
        }
        // ── regs ──
        else if (cmd == "regs") {
            print_regs();
        }
        // ── step / si [N] ──
        else if (cmd == "step" || cmd == "si" || cmd == "s") {
            std::size_t count = 1;
            if (tokens.size() >= 2) {
                if (!parse_dec(tokens[1], count) || count == 0) {
                    log_error("si: invalid count '" + tokens[1]
                              + "' — expected a positive integer, e.g. si 5");
                    continue;
                }
            }
            cmd_step(count);
        }
        // ── run / c ──
        else if (cmd == "run" || cmd == "c") {
            cmd_run();
        }
        // ── x addr [count]  或  x addr=value ──
        else if (cmd == "x") {
            if (tokens.size() < 2) {
                log_error("x: missing argument — usage: x <addr> [count]  or  x <addr>=<value>");
                continue;
            }
            std::uint32_t addr = 0;
            std::uint32_t value = 0;
            bool has_write = false;
            if (!parse_addr_arg(tokens[1], addr, has_write, value)) {
                log_error("x: invalid address '" + tokens[1] + "' — expected hex, e.g. x 0x1000");
                continue;
            }
            if (has_write) {
                cmd_memory_write(addr, value);
                continue;
            }
            std::size_t count = 16;
            if (tokens.size() >= 3) {
                if (!parse_dec(tokens[2], count) || count == 0) {
                    log_error("x: invalid count '" + tokens[2]
                              + "' — expected a positive integer, e.g. x 0x1000 32");
                    continue;
                }
            }
            cmd_memory_examine(addr, count);
        }
        // ── b / break addr ──
        else if (cmd == "b" || cmd == "break") {
            if (tokens.size() < 2) {
                log_error("b: missing address — usage: b <addr>, e.g. b 0x1012c");
                continue;
            }
            std::uint32_t addr = 0;
            if (!parse_hex32(tokens[1], addr)) {
                log_error("b: invalid address '" + tokens[1] + "' — expected hex, e.g. b 0x1012c");
                continue;
            }
            cmd_breakpoint_add(addr);
        }
        // ── del / delete n ──
        else if (cmd == "del" || cmd == "d" || cmd == "delete") {
            if (tokens.size() < 2) {
                log_error("del: missing index — usage: del <n>, e.g. del 0");
                continue;
            }
            std::size_t idx = 0;
            if (!parse_dec(tokens[1], idx)) {
                log_error("del: invalid index '" + tokens[1]
                          + "' — expected a non-negative integer");
                continue;
            }
            cmd_breakpoint_del(idx);
        }
        // ── reset ──
        else if (cmd == "reset") {
            cmd_reset();
        }
        // ── trace on / trace off ──
        else if (cmd == "trace") {
            if (tokens.size() < 2) {
                log_error("trace: missing argument — usage: trace on | trace off");
                continue;
            }
            if (tokens[1] == "on" || tokens[1] == "1") {
                cmd_trace(true);
            } else if (tokens[1] == "off" || tokens[1] == "0") {
                cmd_trace(false);
            } else {
                log_error("trace: invalid argument '" + tokens[1]
                          + "' — expected 'on' or 'off'");
            }
        }
        // ── unknown ──
        else {
            log_error("unknown command: '" + cmd
                      + "' — type 'help' for available commands");
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// 命令实现
// ═══════════════════════════════════════════════════════════════

void Debugger::print_help() {
    std::cout << R"(Commands:
  help / h               Show this help
  quit / q               Exit debugger
  regs / info r          Show all 32 registers (with ABI names) and PC
  si [N] / step [N]      Single-step N instructions (default 1)
  c  / run               Continue execution (until breakpoint or halt)
  x <addr> [count]       Examine memory (hex dump), default 16 bytes
  x <addr>=<value>       Write 32-bit value to memory address
  b <addr> / break <addr> Set breakpoint at address
  info b                 List all breakpoints
  del <n> / d <n>        Delete breakpoint by index
  trace on | off         Enable/disable per-instruction trace log
  reset                  Reset simulator state

All addresses and values are in hex (0x prefix optional).
Examples:
  b 0x1012c              set breakpoint at entry
  si 3                    step 3 instructions
  x 0x10000 32           dump 32 bytes from 0x10000
  x 0x20000=0xdeadbeef   write value to memory
  del 0                  remove breakpoint #0
  trace on               turn on instruction trace
)";
}

void Debugger::print_regs() {
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

void Debugger::cmd_step(std::size_t count) {
    if (count == 0) return;
    log_info("stepping " + std::to_string(count) + " instruction(s)...");

    for (std::size_t i = 0; i < count; ++i) {
        bool ok = simulator_.step();
        if (!ok) {
            log_error("step stopped early at iteration " + std::to_string(i + 1) + "/"
                      + std::to_string(count) + " — simulator returned failure");
            break;
        }
        if (trace_enabled_) {
            std::ostringstream oss;
            oss << "step[" << (i + 1) << "/" << count
                << "] pc=" << hex_str(simulator_.pc());
            log_trace(oss.str());
        }
    }
    print_regs();
}

void Debugger::cmd_run() {
    log_info("continuing execution...");
    simulator_.run(/*max_steps=*/0);
    std::cout << "Execution stopped at pc=" << hex_str(simulator_.pc()) << '\n';
    print_regs();
}

void Debugger::cmd_memory_examine(std::uint32_t addr, std::size_t count) {
    const auto& mem = simulator_.memory();
    if (count == 0) return;

    std::uint32_t row_start = addr & ~0xFu;
    std::size_t rows = (count + (addr - row_start) + 15) / 16;
    if (rows == 0) rows = 1;

    std::cout << std::hex << std::setfill('0');
    for (std::size_t r = 0; r < rows; ++r) {
        std::uint32_t base = row_start + static_cast<std::uint32_t>(r * 16);
        std::cout << "0x" << std::setw(8) << base << "  ";

        // hex bytes
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

        // ASCII
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

void Debugger::cmd_memory_write(std::uint32_t addr, std::uint32_t value) {
    auto& mem = simulator_.memory();
    if (!mem.store32(addr, value)) {
        log_error("memory write failed: address " + hex_str(addr) + " is out of range");
        return;
    }
    std::cout << "Wrote " << hex_str(value) << " to " << hex_str(addr) << '\n';
}

void Debugger::cmd_breakpoint_add(std::uint32_t addr) {
    auto it = std::find_if(breakpoints_.begin(), breakpoints_.end(),
        [addr](const Breakpoint& bp) { return bp.addr == addr; });
    if (it != breakpoints_.end()) {
        std::cout << "Breakpoint #" << it->id << " already exists at "
                  << hex_str(addr) << (it->enabled ? " (enabled)" : " (disabled)") << '\n';
        return;
    }

    Breakpoint bp{next_bp_id_++, addr, true};
    breakpoints_.push_back(bp);
    std::cout << "Breakpoint #" << bp.id << " set at " << hex_str(addr) << '\n';
}

void Debugger::cmd_breakpoint_del(std::size_t index) {
    if (index >= breakpoints_.size()) {
        log_error("del: breakpoint [" + std::to_string(index) + "] out of range — "
                  + std::to_string(breakpoints_.size()) + " breakpoint(s) exist, "
                  + "valid range: [0, " + std::to_string(breakpoints_.empty() ? 0 : breakpoints_.size() - 1) + "]");
        return;
    }

    const auto& bp = breakpoints_[index];
    std::cout << "Deleted breakpoint #" << bp.id << " at " << hex_str(bp.addr) << '\n';
    breakpoints_.erase(breakpoints_.begin() + static_cast<std::ptrdiff_t>(index));
}

void Debugger::cmd_breakpoint_list() {
    if (breakpoints_.empty()) {
        std::cout << "No breakpoints. Use 'b <addr>' to set one.\n";
        return;
    }

    std::cout << "Breakpoints (" << breakpoints_.size() << "):\n";
    for (std::size_t i = 0; i < breakpoints_.size(); ++i) {
        const auto& bp = breakpoints_[i];
        std::cout << "  [" << std::dec << i << "] "
                  << "#" << bp.id << "  "
                  << hex_str(bp.addr)
                  << "  " << (bp.enabled ? "enabled" : "disabled") << '\n';
    }
}

void Debugger::cmd_reset() {
    simulator_.reset();
    breakpoints_.clear();
    next_bp_id_ = 1;
    trace_enabled_ = false;
    log_info("simulator reset complete (breakpoints cleared, trace off)");
    print_regs();
}

void Debugger::cmd_trace(bool on) {
    trace_enabled_ = on;
    std::cout << "Instruction trace: " << (on ? "ON" : "OFF") << '\n';
}

}  // namespace riscv
