# 调试器使用手册

## 概述

RISC-V 调试器是交互式命令行工具，通过 `handle_command()` 方法处理用户输入、控制模拟器执行、查看寄存器与内存状态。

## 架构说明

```
用户输入 → repl() → handle_command() → 命令分发
                                         ├─ print_registers()
                                         ├─ print_memory()
                                         ├─ list_breakpoints()
                                         ├─ add/remove_breakpoint()
                                         ├─ simulator_.step() / run() / reset()
                                         └─ 返回 DebugCommandResult {handled, should_exit, message}
```

`DebugCommandResult` 结构体：

```cpp
struct DebugCommandResult {
    bool handled = false;    // 命令是否被识别
    bool should_exit = false; // 是否退出 REPL
    std::string message;     // 确认消息或错误提示
};
```

- `handled=true` 且 `message` 为空 → 命令直接输出（如 `regs`）
- `handled=true` 且 `message` 非空 → 由 `repl()` 打印 message
- `should_exit=true` → `repl()` 退出循环

## 所有命令

### `help` / `h`

打印命令列表。

### `quit` / `q`

退出调试器。返回 `{true, true, ""}`。

### `regs` / `info r`

打印全部 32 个寄存器（x0–x31）+ ABI 名称 + PC。

**输出格式**：
```
PC  = 0x0001012c

x 0(zero)=0x00000000  x16(a6  )=0x00000000
...
x15(a5  )=0x00000000  x31(t6  )=0x00000000
```

### `info b`

列出所有断点。无断点时显示 "No breakpoints."。

### `si [N]` / `step [N]` / `s [N]`

单步执行 N 条指令。不传参数默认 1 条。

- 每步调用 `simulator_.step()`
- 步进失败时立即停止，返回错误信息（含 `last_error()`）
- 完成后自动打印寄存器状态

**错误示例**：`si: stopped early at step 1/3 — simulator is not ready`

### `c` / `run`

继续执行直到：
1. 命中活跃断点（CPU 循环中 `has_breakpoint(pc)` 返回 true）
2. 程序正常退出（`exit` syscall）
3. 停机指令（`ebreak`）
4. 执行错误（非法指令等）

停止后打印状态和 PC，同时输出寄存器快照。

### `x <addr> [count]`

内存 hex dump（16 字节对齐行）。

- 不传 count 默认 16 字节
- `??` 表示越界或未映射地址
- 右侧 ASCII 列，不可打印字符显示 `.`

### `x <addr>=<value>`

向内存地址写入 32 位值。小端序存储。

### `b <addr>` / `break <addr>`

在指定地址设断点。由 `Simulator::add_breakpoint()` 管理，重复地址会拒绝。

### `del <n>` / `d <n>` / `delete <n>`

按索引删除断点。索引来自 `info b` 列表。

### `trace on` / `trace off`

开关指令执行跟踪。状态由 `Simulator::set_trace_enabled()` 保存到 `SimulatorConfig::enable_trace`。

### `reset`

重置模拟器状态：寄存器清零、PC 归零、清除断点、清除错误。

---

## 提供给 A 的公开接口

调试器通过 `Simulator` 访问一切状态。A 成员只需保证以下接口正确：

```cpp
// 已在 simulator.hpp 中定义：
bool Simulator::step();                         // 单步执行
void Simulator::run(std::size_t max_steps);     // 连续运行
bool Simulator::reset();                        // 重置
uint32_t Simulator::pc() const;                 // 当前 PC
const uint32_t* Simulator::regs() const;        // 寄存器数组
Memory& Simulator::memory();                    // 内存访问
bool Simulator::add_breakpoint(uint32_t addr);  // 添加断点
bool Simulator::remove_breakpoint(uint32_t addr); // 删除断点
bool Simulator::has_breakpoint(uint32_t addr) const; // 断点检查
const vector<uint32_t>& Simulator::breakpoints() const; // 断点列表
void Simulator::set_trace_enabled(bool);        // trace 开关
SimulatorStatus Simulator::status() const;      // 状态查询
```

---

## 错误处理设计

所有非法输入返回明确错误提示，程序不崩溃：

| 场景 | 返回消息 |
|---|---|
| 空命令 | 静默忽略 |
| 未知命令 | `unknown command` |
| 缺参数 | `b: missing address` / `trace: missing argument` 等 |
| 非法格式 | `b: invalid address` / `si: invalid count 'abc'` 等 |
| 越界 | `del: out of range` |
| 写失败 | `x: write failed at 0x... — address out of range` |
| 步进失败 | `si: stopped early at step N/M — <error from simulator>` |
