# 调试器使用手册

## 概述

RISC-V 调试器是交互式命令行工具，在主菜单中选择 `2` 进入。通过 `handle_command()` 方法处理用户输入、控制模拟器执行、查看寄存器与内存状态。

**启动流程**：

```
./build/riscv_sim                          # 启动模拟器
→ RISC-V Simulator Menu                    # 主菜单
  → 1. Load ELF program                   # 加载 ELF 程序
  → 2. Enter debugger shell               # 进入调试 Shell
  → 3. Show current program path          # 查看已加载程序
  → 4. Quit                               # 退出
```

输入 `2` 后进入调试 Shell，看到 `RISC-V debugger shell ready.` 提示符，所有调试命令在此运行。`quit` 返回主菜单，主菜单选 `4` 退出程序。

---

## 架构说明

```
用户输入 → repl() → handle_command() → 命令分发
                                         ├─ print_registers()      32 寄存器 + PC
                                         ├─ print_status()         PC + 状态 + 退出码
                                         ├─ print_pipeline()       五级流水线 + CPI
                                         ├─ print_memory()         hex dump + ASCII
                                         ├─ list_breakpoints()     断点列表
                                         ├─ add/remove_breakpoint()
                                         ├─ simulator_.step() / run() / reset()
                                         └─ 返回 DebugCommandResult {handled, should_exit, message}
```

`DebugCommandResult` 结构体：

```cpp
struct DebugCommandResult {
    bool handled = false;      // 命令是否被识别
    bool should_exit = false;  // 是否退出 REPL（返回主菜单）
    std::string message;       // 确认消息或错误提示
};
```

---

## 全部命令（15 个）

### `help` / `h`

打印完整命令列表。

### `quit` / `q`

退出调试 Shell，返回主菜单。

### `regs`

打印全部 32 个通用寄存器（x0–x31）+ ABI 名称 + PC。4 列紧凑格式。

### `info r`

同 `regs`。

### `info b`

列出所有断点。无断点时显示 `No breakpoints.`。

### `step [N]` / `s [N]`

单步执行 N 条指令。不传参数默认 1 条。失败立即停止并报原因。

### `continue` / `c` / `run`

继续执行直到断点、exit、ebreak 或错误。停止后打印状态。

### `demo`

自动演示完整调试流程（6 步）：

```
[1/6] 设断点 → [2/6] 运行到断点 → [3/6] 打印寄存器
→ [4/6] 单步 3 条 → [5/6] 查看内存 → [6/6] 清理断点 → 继续到退出
```

**前提**：需先通过主菜单选项 1 加载 ELF 程序。

### `pipeline` / `pipe` / `cpi`

五级流水线状态 + CPI 统计（选做功能）。输出 cycles/instructions/stalls/flushes/CPI + IF/ID/EX/MEM/WB 各级状态。

### `x <addr> [count]`

内存 hex dump（16 字节对齐行）。默认 16 字节。`??` 标记越界。

### `x <addr>=<value>`

向内存地址写入 32 位值（小端序）。

### `b <addr>` / `break <addr>`

设断点。重复地址自动拒绝。

### `del <n>` / `d <n>`

按索引删除断点。

### `trace on` / `trace off`

开关指令跟踪。

### `reset`

重置模拟器：寄存器清零、PC 归零、清除断点和流水线统计。

---

## 核心四件套（必做验收）

| 命令 | 功能 | 验收标准 |
|---|---|---|
| `step` / `s` | 单步执行 | PC 正确推进，失败有原因提示 |
| `continue` / `c` | 继续运行 | 遇断点暂停、exit 正常退出 |
| `info r` / `regs` | 寄存器查看 | 32 寄存器 + ABI 名 + PC |
| `x <addr>` | 内存查看 | hex dump 格式正确，`??` 标记越界 |

## 演示命令（一键答辩）

`demo` 命令等价于执行：
```
b <entry> → c → regs → step 3 → x <pc> 32 → info b → del → c
```

---

## 接口约定（供 A 使用）

```cpp
// simulator.hpp 中已定义：
bool Simulator::step();
void Simulator::run(std::size_t max_steps = 0);
uint32_t Simulator::pc() const;
uint32_t Simulator::reg(std::size_t index) const;
Memory& Simulator::memory();
bool Simulator::add/remove/has_breakpoint(uint32_t addr);
const vector<uint32_t>& Simulator::breakpoints() const;
void Simulator::set_trace_enabled(bool);
SimulatorStatus Simulator::status() const;
PipelineStats Simulator::pipeline_stats() const;
const PipelineSnapshot& Simulator::pipeline_snapshot() const;
```

## 错误处理

所有非法输入不崩溃，返回明确提示：

| 场景 | 返回消息 |
|---|---|
| 空命令 | 静默忽略 |
| 未知命令 | `unknown command` |
| 缺参 | `b: missing address` 等 |
| 非法格式 | `b: invalid address` |
| 越界 | `del: out of range` |
| 写入失败 | `x: write failed at 0x... — address out of range` |
| 步进失败 | `step: stopped at N/M — <reason>` |
