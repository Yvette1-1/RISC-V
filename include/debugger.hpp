#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace riscv {

class Simulator;

struct Breakpoint {
    std::uint32_t id;
    std::uint32_t addr;
    bool enabled;
};

class Debugger {
public:
    explicit Debugger(Simulator& simulator);

    void repl();

    /// 供 CPU 执行循环调用：当前 PC 是否有活跃断点
    bool has_breakpoint(std::uint32_t pc) const;

    /// 供外部查询：是否开启指令跟踪日志
    bool trace_enabled() const noexcept { return trace_enabled_; }

private:
    void print_help();
    void print_regs();
    void cmd_step(std::size_t count = 1);
    void cmd_run();
    void cmd_memory_examine(std::uint32_t addr, std::size_t count);
    void cmd_memory_write(std::uint32_t addr, std::uint32_t value);
    void cmd_breakpoint_add(std::uint32_t addr);
    void cmd_breakpoint_del(std::size_t index);
    void cmd_breakpoint_list();
    void cmd_reset();
    void cmd_trace(bool on);

    Simulator& simulator_;
    std::vector<Breakpoint> breakpoints_;
    std::uint32_t next_bp_id_ = 1;
    bool trace_enabled_ = false;
};

}  // namespace riscv
