#include "debugger.hpp"
#include "riscv_config.hpp"
#include "simulator.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

void init_console_utf8() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void print_banner() {
    std::cout << "\n============================================================\n";
    std::cout << "  RISC-V 模拟器与调试器\n";
    std::cout << "  面向教学的 ELF 装载、交互调试、流水线统计与 GDB 接入\n";
    std::cout << "============================================================\n";
}

void print_menu() {
    std::cout << "\n[主菜单]\n";
    std::cout << "  1. 装载并运行 ELF 程序\n";
    std::cout << "  2. 进入调试器\n";
    std::cout << "  3. 查看 GDB 远程调试状态\n";
    std::cout << "  4. 查看快速示例\n";
    std::cout << "  5. 运行完整 riscv-tests 验证\n";
    std::cout << "  6. 退出\n";
    std::cout << "请输入选项编号：";
}

void print_examples() {
    std::cout << "\n[快速示例]\n";
    std::cout << "  demo                   一键演示断点、单步、寄存器与内存查看\n";
    std::cout << "  b 0x1012c             设置断点\n";
    std::cout << "  step 3                单步执行三条指令\n";
    std::cout << "  pipeline              查看流水线状态与 CPI\n";
    std::cout << "  x 0x10000 32          查看 32 字节内存\n";
    std::cout << "  x 0x20000=0xdeadbeef  向内存写入 32 位数据\n";
    std::cout << "  make rv32ui           编译 riscv-tests 的 RV32UI 套件\n";
    std::cout << "  make rv32um           编译 riscv-tests 的 RV32UM 套件\n";
    std::cout << "  make rv32uf           编译 riscv-tests 的 RV32UF 套件\n";
    std::cout << "\n";
}

std::string prompt_path() {
    std::cout << "Enter ELF path: ";
    std::string path;
    std::getline(std::cin, path);
    return path;
}

bool load_program_with_feedback(riscv::Simulator& simulator, const std::string& path) {
    if (simulator.load_program(path)) {
        std::cout << "Loaded: " << path << '\n';
        std::cout << "Entry PC: 0x" << std::hex << simulator.pc() << std::dec << '\n';
        return true;
    }
    std::cout << "Failed to load program: " << path << '\n';
    std::cout << "Reason: " << simulator.last_error() << '\n';
    return false;
}

bool run_command(const std::string& command, const std::string& cwd) {
#ifdef _WIN32
    const std::string full = "cd /d \"" + cwd + "\" && " + command;
#else
    const std::string full = "cd \"" + cwd + "\" && " + command;
#endif
    return std::system(full.c_str()) == 0;
}

struct RiscvTestCase {
    std::string suite;
    std::filesystem::path path;
};

std::vector<RiscvTestCase> collect_riscv_tests(const std::filesystem::path& dir, const std::string& prefix) {
    std::vector<RiscvTestCase> cases;
    if (!std::filesystem::exists(dir)) {
        return cases;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0 && !entry.path().has_extension()) {
            cases.push_back({prefix, entry.path()});
        }
    }
    std::sort(cases.begin(), cases.end(), [](const RiscvTestCase& a, const RiscvTestCase& b) {
        return a.path < b.path;
    });
    return cases;
}

struct CaseRunResult {
    enum class Kind { Pass, Fail, Timeout } kind = Kind::Fail;
    std::string detail;
    std::uint64_t elapsed_ms = 0;
    std::vector<std::uint32_t> pc_trace;
};

std::string format_pc(std::uint32_t pc) {
    std::ostringstream oss;
    oss << "0x" << std::hex << pc;
    return oss.str();
}

std::string format_trace(const std::vector<std::uint32_t>& trace) {
    if (trace.empty()) {
        return "[]";
    }
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0; i < trace.size(); ++i) {
        if (i != 0) oss << " -> ";
        oss << format_pc(trace[i]);
    }
    oss << ']';
    return oss.str();
}

CaseRunResult run_case(const std::filesystem::path& elf_path, riscv::Simulator& simulator, std::size_t max_steps = 100'000, std::size_t progress_interval = 10'000) {
    CaseRunResult result;
    const auto started = std::chrono::steady_clock::now();
    if (!simulator.load_program(elf_path.string())) {
        result.detail = "load failed: " + simulator.last_error();
        result.kind = CaseRunResult::Kind::Fail;
        result.elapsed_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
        return result;
    }

    std::vector<std::uint32_t> trace;
    trace.reserve(8);
    std::size_t steps = 0;
    while (steps < max_steps) {
        trace.push_back(simulator.pc());
        if (trace.size() > 8) {
            trace.erase(trace.begin());
        }
        if (progress_interval != 0 && steps != 0 && (steps % progress_interval) == 0) {
            const auto status = simulator.status();
            std::cout << "[PROGRESS] " << elf_path.filename().string()
                      << " steps=" << steps
                      << " pc=" << format_pc(status.pc)
                      << " state=" << static_cast<int>(status.state)
                      << '\n';
        }

        const auto st = simulator.state();
        if (st == riscv::SimulatorState::Exited || st == riscv::SimulatorState::Halted) {
            result.kind = CaseRunResult::Kind::Pass;
            break;
        }
        if (!simulator.step()) {
            const auto status = simulator.status();
            result.kind = CaseRunResult::Kind::Fail;
            result.detail = "state=" + std::to_string(static_cast<int>(status.state)) +
                            ", pc=" + format_pc(status.pc) +
                            ", error=" + status.message;
            break;
        }
        ++steps;
        const auto status = simulator.status();
        if (status.state == riscv::SimulatorState::Exited || status.state == riscv::SimulatorState::Halted) {
            result.kind = CaseRunResult::Kind::Pass;
            break;
        }
        if (status.state == riscv::SimulatorState::Paused) {
            result.kind = CaseRunResult::Kind::Timeout;
            result.detail = "timeout after " + std::to_string(max_steps) + " steps, state=" + std::to_string(static_cast<int>(status.state)) +
                            ", pc=" + format_pc(status.pc) +
                            ", error=" + status.message +
                            ", trace=" + format_trace(trace);
            break;
        }
    }

    if (result.kind == CaseRunResult::Kind::Fail && result.detail.empty()) {
        const auto status = simulator.status();
        result.detail = "state=" + std::to_string(static_cast<int>(status.state)) +
                        ", pc=" + format_pc(status.pc) +
                        ", error=" + status.message;
    }
    if (steps >= max_steps && result.kind != CaseRunResult::Kind::Pass) {
        const auto status = simulator.status();
        result.kind = CaseRunResult::Kind::Timeout;
        result.detail = "timeout after " + std::to_string(max_steps) + " steps, state=" + std::to_string(static_cast<int>(status.state)) +
                        ", pc=" + format_pc(status.pc) +
                        ", error=" + status.message +
                        ", trace=" + format_trace(trace);
    }

    result.elapsed_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
    result.pc_trace = std::move(trace);
    return result;
}

bool run_riscv_suite(const std::string& suite_name) {
    const std::string project_root = "D:/1-RISC-V/RISC-V";
    const std::string isa_dir = project_root + "/tests/riscv-tests/isa";
    const std::string make_cmd = "make RISCV_PREFIX=riscv-none-elf- XLEN=32 " + suite_name;

    std::cout << "\n============================================================\n";
    std::cout << "[" << suite_name << "]\n";
    std::cout << "============================================================\n";

    if (!run_command(make_cmd, isa_dir)) {
        std::cout << "[BUILD] FAIL  " << suite_name << " (make failed)\n";
        return false;
    }

    const auto tests = collect_riscv_tests(isa_dir, suite_name + "-p-");
    if (tests.empty()) {
        std::cout << "[DISCOVERY] FAIL  no ELF files found for " << suite_name << "\n";
        return false;
    }

    riscv::Simulator simulator({});
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t timed_out = 0;
    for (const auto& test : tests) {
        std::cout << "[RUN]     " << test.path.filename().string() << '\n';
        const auto r = run_case(test.path, simulator);
        switch (r.kind) {
        case CaseRunResult::Kind::Pass:
            ++passed;
            std::cout << "[PASS]    " << test.path.filename().string() << "  (" << r.elapsed_ms << " ms)\n";
            break;
        case CaseRunResult::Kind::Timeout:
            ++timed_out;
            std::cout << "[TIMEOUT] " << test.path.filename().string() << "  (" << r.elapsed_ms << " ms, " << r.detail << ")\n";
            break;
        case CaseRunResult::Kind::Fail:
            ++failed;
            std::cout << "[FAIL]    " << test.path.filename().string() << "  (" << r.elapsed_ms << " ms, " << r.detail << ")\n";
            break;
        }
    }

    const auto total = tests.size();
    const auto percent = total == 0 ? 0 : (passed * 100 / total);
    std::cout << "[SUMMARY] " << suite_name << ": pass=" << passed << ", fail=" << failed << ", timeout=" << timed_out << ", total=" << total << " (" << percent << "%)\n";
    return passed == total;
}

bool run_riscv_tests_suite() {
    const std::vector<std::string> suites = {"rv32ui", "rv32um", "rv32uf"};
    std::size_t total_cases = 0;
    std::size_t total_passed = 0;
    bool ok = true;

    std::cout << "\n============================================================\n";
    std::cout << "完整 riscv-tests 套件回归\n";
    std::cout << "============================================================\n";

    for (const auto& suite : suites) {
        const std::string project_root = "D:/1-RISC-V/RISC-V";
        const std::string isa_dir = project_root + "/tests/riscv-tests/isa";
        const std::string make_cmd = "make RISCV_PREFIX=riscv-none-elf- XLEN=32 " + suite;
        std::cout << "\n[BUILD] " << suite << '\n';
        if (!run_command(make_cmd, isa_dir)) {
            std::cout << "[BUILD] FAIL  " << suite << '\n';
            ok = false;
            continue;
        }

        const auto tests = collect_riscv_tests(isa_dir, suite + "-p-");
        if (tests.empty()) {
            std::cout << "[DISCOVERY] FAIL  no ELF files found for " << suite << '\n';
            ok = false;
            continue;
        }

        std::cout << "[RUN] " << suite << " (" << tests.size() << " cases)\n";
        riscv::Simulator simulator({});
        std::size_t passed = 0;
        std::size_t failed = 0;
        std::size_t timed_out = 0;
        for (const auto& test : tests) {
            std::cout << "[RUN]     " << test.path.filename().string() << '\n';
            const auto r = run_case(test.path, simulator);
            switch (r.kind) {
            case CaseRunResult::Kind::Pass:
                ++passed;
                std::cout << "[PASS]    " << test.path.filename().string() << '\n';
                break;
            case CaseRunResult::Kind::Timeout:
                ++timed_out;
                std::cout << "[TIMEOUT] " << test.path.filename().string() << "  (" << r.detail << ")\n";
                break;
            case CaseRunResult::Kind::Fail:
                ++failed;
                std::cout << "[FAIL]    " << test.path.filename().string() << "  (" << r.detail << ")\n";
                break;
            }
        }

        total_cases += tests.size();
        total_passed += passed;
        if (passed != tests.size()) {
            ok = false;
        }
        std::cout << "[SUMMARY] " << suite << ": pass=" << passed << ", fail=" << failed << ", timeout=" << timed_out << ", total=" << tests.size() << '\n';
    }

    const auto percent = total_cases == 0 ? 0 : (total_passed * 100 / total_cases);
    std::cout << "\n============================================================\n";
    std::cout << "[TOTAL] " << total_passed << "/" << total_cases << " passed (" << percent << "%)\n";
    std::cout << "============================================================\n";
    std::cout << "提示：rv32uf 若失败，优先查看是否为浮点结果、NaN 传播或 FPU 标志位问题。\n";
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    init_console_utf8();
    print_banner();

    riscv::SimulatorConfig config;
    riscv::Simulator simulator(config);

    if (argc > 1) {
        load_program_with_feedback(simulator, argv[1]);
    }

    riscv::Debugger debugger(simulator);

    while (true) {
        print_menu();
        int choice = 0;
        if (!(std::cin >> choice)) {
            break;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 1) {
            const auto path = prompt_path();
            if (path.empty()) {
                std::cout << "未输入路径。\n";
                continue;
            }
            load_program_with_feedback(simulator, path);
        } else if (choice == 2) {
            std::cout << "\n正在进入调试器……\n";
            std::cout << "输入 help 可查看全部命令，输入 demo 可一键演示完整流程。\n";
            debugger.repl();
        } else if (choice == 3) {
            std::cout << "\nGDB 远程调试功能已集成到模拟器后端。\n";
            std::cout << "可在需要时使用远程客户端连接到配置端口。\n";
        } else if (choice == 4) {
            print_examples();
        } else if (choice == 5) {
            const bool ok = run_riscv_tests_suite();
            if (!ok) {
                std::cout << "\n部分 riscv-tests 套件编译失败，请检查工具链或测试目录。\n";
            }
        } else if (choice == 6) {
            break;
        } else {
            std::cout << "Unknown option.\n";
        }
    }

    return 0;
}
