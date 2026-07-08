#include "debugger.hpp"
#include "simulator.hpp"

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace riscv {

namespace {

std::string hex_str(std::uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << v;
    return oss.str();
}

bool parse_addr_write(const std::string& token,
                      std::uint32_t& addr,
                      bool& has_write,
                      std::uint32_t& value) {
    has_write = false;
    const auto eq = token.find('=');
    if (eq == std::string::npos) {
        return Debugger::parse_address(token, addr);
    }
    if (eq == 0 || eq + 1 >= token.size()) {
        return false;
    }
    if (!Debugger::parse_address(token.substr(0, eq), addr)) {
        return false;
    }
    if (!Debugger::parse_address(token.substr(eq + 1), value)) {
        return false;
    }
    has_write = true;
    return true;
}

}  // namespace

Debugger::Debugger(Simulator& simulator) : simulator_(simulator) {}

void Debugger::repl() {
    std::cout << "\n============================================================\n";
    std::cout << "  调试器\n";
    std::cout << "============================================================\n";
    std::cout << "输入 help 查看命令说明。\n";
    std::cout << "输入 demo 可一键演示断点、单步、寄存器与内存查看。\n\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        const auto result = handle_command(line);
        if (!result.message.empty()) {
            std::cout << result.message << '\n';
        }
        if (result.should_exit) {
            std::cout << "Leaving debugger shell.\n";
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
        return {true, false, "info: unknown subcommand '" + sub + "'"};
    }
    if (cmd == "step" || cmd == "si" || cmd == "s") {
        std::size_t count = 1;
        std::string arg;
        if (iss >> arg) {
            char* end = nullptr;
            const auto parsed = std::strtoul(arg.c_str(), &end, 10);
            if (end == arg.c_str() || *end != '\0' || parsed == 0) {
                return {true, false, "step: invalid count '" + arg + "'"};
            }
            count = static_cast<std::size_t>(parsed);
        }
        for (std::size_t i = 0; i < count; ++i) {
            if (!simulator_.step()) {
                std::ostringstream msg;
                msg << "step: stopped at " << (i + 1) << '/' << count << " — "
                    << (simulator_.last_error().empty() ? "Step failed" : simulator_.last_error());
                return {true, false, msg.str()};
            }
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
    if (cmd == "demo") {
        const auto& path = simulator_.program_path();
        if (path.empty()) {
            return {true, false, "demo: no program loaded — load ELF first via menu option 1"};
        }

        const auto entry = simulator_.pc();
        std::cout << "\n========== Demo: " << path << " ==========\n\n";
        std::cout << "[1/6] Setting breakpoint at entry " << hex_str(entry) << '\n';
        add_breakpoint(entry);

        std::cout << "[2/6] Running to breakpoint...\n";
        simulator_.run();
        print_status();

        std::cout << "\n[3/6] Register dump:\n";
        print_registers();

        std::cout << "\n[4/6] Stepping 3 instructions:\n";
        for (int i = 0; i < 3; ++i) {
            if (!simulator_.step()) {
                std::cout << "  step stopped: " << simulator_.last_error() << '\n';
                break;
            }
            print_status();
        }

        std::cout << "\n[5/6] Memory around PC:\n";
        print_memory(simulator_.pc(), 32);

        std::cout << "\n[6/6] Breakpoints:\n";
        list_breakpoints();
        remove_breakpoint(entry);
        std::cout << "Continuing execution...\n";
        simulator_.run();
        print_status();
        std::cout << "\n========== Demo complete ==========\n";
        return {true, false, {}};
    }
    if (cmd == "x") {
        std::string tok;
        if (!(iss >> tok)) {
            return {true, false, "x: missing address — usage: x <addr> [count]  or  x <addr>=<value>"};
        }

        std::uint32_t addr = 0;
        std::uint32_t value = 0;
        bool has_write = false;
        if (!parse_addr_write(tok, addr, has_write, value)) {
            return {true, false, "x: invalid address '" + tok + "'"};
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
            const auto parsed = std::strtoul(cnt_arg.c_str(), &end, 10);
            if (end == cnt_arg.c_str() || *end != '\0' || parsed == 0) {
                return {true, false, "x: invalid count '" + cnt_arg + "'"};
            }
            count = static_cast<std::size_t>(parsed);
        }
        print_memory(addr, count);
        return {true, false, {}};
    }
    if (cmd == "trace") {
        std::string mode;
        if (!(iss >> mode)) {
            return {true, false, "trace: missing argument"};
        }
        simulator_.set_trace_enabled(mode == "on");
        return {true, false, mode == "on" ? "Instruction trace: ON" : "Instruction trace: OFF"};
    }
    if (cmd == "b" || cmd == "break") {
        std::string tok;
        if (!(iss >> tok)) {
            return {true, false, "b: missing address"};
        }
        std::uint32_t addr = 0;
        if (!parse_address(tok, addr)) {
            return {true, false, "b: invalid address"};
        }
        add_breakpoint(addr);
        return {true, false, "Breakpoint set"};
    }
    if (cmd == "del" || cmd == "d" || cmd == "delete") {
        std::size_t idx = 0;
        if (!(iss >> idx)) {
            return {true, false, "del: missing index"};
        }
        const auto& bps = simulator_.breakpoints();
        if (idx >= bps.size()) {
            return {true, false, "del: out of range"};
        }
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
    std::cout << R"(可用命令：
  help / h                显示本帮助
  quit / q                退出调试器
  regs / r                查看全部寄存器与 PC
  info r                  同 regs
  info b                  查看断点列表
  step [N] / si [N] / s [N]
                          单步执行 N 条指令（默认 1 条）
  continue / c / run      持续运行，直到断点、异常或停机
  demo                    一键演示断点、单步、寄存器与内存查看
  pipeline / pipe / cpi   查看流水线状态与 CPI 统计
  x <addr> [count]        查看内存（十六进制转储）
  x <addr>=<value>        向内存写入 32 位数据
  b <addr> / break        设置断点
  del <n> / d <n>         按编号删除断点
  trace on | off          开关指令跟踪
  reset                   重置模拟器状态

示例：
  demo
  b 0x1012c
  step 3
  x 0x10000 32
  x 0x20000=0xdeadbeef
  del 0
)";
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
    std::cout << "cycles=" << stats.cycles << " instructions=" << stats.instructions
              << " stalls=" << stats.stalls << " flushes=" << stats.flushes
              << " CPI=" << std::fixed << std::setprecision(3) << stats.cpi << '\n';
    std::cout << std::hex;
    std::cout << "IF  " << (pipe.if_valid ? "0x" : "--") << (pipe.if_valid ? pipe.if_pc : 0) << '\n';
    std::cout << "ID  " << (pipe.id_valid ? "0x" : "--") << (pipe.id_valid ? pipe.id_pc : 0) << '\n';
    std::cout << "EX  " << (pipe.ex_valid ? "0x" : "--") << (pipe.ex_valid ? pipe.ex_pc : 0) << '\n';
    std::cout << "MEM " << (pipe.mem_valid ? "0x" : "--") << (pipe.mem_valid ? pipe.mem_pc : 0) << '\n';
    std::cout << "WB  " << (pipe.wb_valid ? "0x" : "--") << (pipe.wb_valid ? pipe.wb_pc : 0) << std::dec << '\n';
}

void Debugger::print_memory(std::uint32_t addr, std::size_t count) const {
    if (count == 0) {
        return;
    }
    const auto& mem = simulator_.memory();
    const std::uint32_t row_start = addr & ~0xFu;
    std::size_t rows = (count + (addr - row_start) + 15) / 16;
    if (rows == 0) {
        rows = 1;
    }

    std::cout << std::hex << std::setfill('0');
    for (std::size_t r = 0; r < rows; ++r) {
        const std::uint32_t base = row_start + static_cast<std::uint32_t>(r * 16);
        std::cout << "0x" << std::setw(8) << base << "  ";

        for (int i = 0; i < 16; ++i) {
            const std::uint32_t a = base + static_cast<std::uint32_t>(i);
            std::uint8_t byte = 0;
            if (mem.load8(a, byte)) {
                std::cout << std::setw(2) << static_cast<unsigned>(byte) << ' ';
            } else {
                std::cout << "?? ";
            }
        }

        std::cout << " |";
        for (int i = 0; i < 16; ++i) {
            const std::uint32_t a = base + static_cast<std::uint32_t>(i);
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
        std::cout << "  [" << std::dec << i << "] " << hex_str(bps[i]) << '\n';
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

bool Debugger::parse_address(const std::string& token, std::uint32_t& value) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    const auto parsed = std::strtoul(token.c_str(), &end, 0);
    if (end == token.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace riscv
