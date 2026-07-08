#include "debugger.hpp"
#include "riscv_config.hpp"
#include "simulator.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <limits>
#include <string>

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
    std::cout << "  5. 运行 riscv-tests 验证\n";
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
            std::cout << "\n[运行 riscv-tests]\n";
            std::cout << "请先进入 tests\\riscv-tests\\isa 目录，然后执行：\n";
            std::cout << "  make RISCV_PREFIX=riscv-none-elf- XLEN=32 rv32ui\n";
            std::cout << "\n运行完成后，可回到项目根目录使用 ctest 或直接加载生成的 ELF 进行验证。\n";
        } else if (choice == 6) {
            break;
        } else {
            std::cout << "Unknown option.\n";
        }
    }

    return 0;
}
