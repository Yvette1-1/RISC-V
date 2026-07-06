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

输入 `2` 后进入调试 Shell，看到 `RISC-V debugger shell ready.` 提示符，所有调试命令在此运行。输入 `quit` 返回主菜单，在主菜单选 `4` 退出程序。

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

## 全部命令

### `help` / `h`

打印完整命令列表。

### `quit` / `q`

退出调试 Shell，返回主菜单。

### `regs`

打印全部 32 个通用寄存器（x0–x31）+ ABI 名称 + PC。4 列紧凑格式。

**输出格式**：
```
PC = 0x0001012c
x0/zero  = 0x00000000  x1/ra    = 0x00000000  x2/sp    = 0x7fff0000  x3/gp    = 0x00000000
x4/tp    = 0x00000000  x5/t0    = 0x00000000  x6/t1    = 0x00000000  x7/t2    = 0x00000000
...
x28/t3   = 0x00000000  x29/t4   = 0x00000000  x30/t5   = 0x00000000  x31/t6   = 0x00000000
```

x0 恒为 0（RISC-V 规范）。

### `info r`

同 `regs`。

### `info b`

列出所有断点。无断点时显示 `No breakpoints.`。

**输出格式**：
```
Breakpoints (2):
  [0] 0x0001012c
  [1] 0x00010140
```

`[索引]` 用于 `del` 删除。

---

### `step [N]` / `s [N]`

单步执行 N 条指令。不传参数默认 1 条。

- 每步调用 `simulator_.step()`
- 步进失败时立即停止，返回 `step: stopped at N/M — <原因>`
- 完成后打印 `PC = ...` 状态行
- 已加载程序时正常推进 PC，无程序时提示 `simulator is not ready`

**示例**：
```
step        # 走 1 条
step 5      # 走 5 条
s 3         # 走 3 条
```

### `continue` / `c` / `run`

继续执行直到：
1. 命中活跃断点（CPU 循环中 `has_breakpoint(pc)` 返回 true）
2. 程序正常退出（`exit` syscall）→ 显示 `exited with code N`
3. 停机指令（`ebreak`）
4. 执行错误

停止后打印 `PC = ... (状态描述)`。

### `pipeline` / `pipe` / `cpi`

打印五级流水线状态快照和 CPI 统计（A 的选做功能）。

**输出格式**：
```
cycles=10 instructions=10 stalls=0 flushes=2 CPI=1.000
IF  0x80000028
ID  --
EX  0x80000020
MEM 0x8000001c
WB  0x80000018
```

各级无效时显示 `--`。

---

### `x <addr> [count]`

内存 hex dump（16 字节对齐行）。

- 不传 count 默认 16 字节
- `??` 表示越界或未映射地址
- 右侧 ASCII 列，不可打印字符显示 `.`

**输出格式**：
```
0x00000000  00 00 00 00 12 34 56 78 ff ff 00 00 00 00 00 00  |......4Vx........|
0x00000010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |................|
```

### `x <addr>=<value>`

向内存地址写入 32 位值。小端序存储，自动确认写入结果。

```
> x 0x100=0xdeadbeef
Wrote 0xdeadbeef to 0x00000100
> x 0x100 4
0x00000100  ef be ad de 00 00 00 00 ...  |....|
```

---

### `b <addr>` / `break <addr>`

在指定地址设断点。`Simulator::add_breakpoint()` 管理，重复地址拒绝。

### `del <n>` / `d <n>`

按索引删除断点。索引来自 `info b` 列表。

### `trace on` / `trace off`

开关指令执行跟踪。状态存入 `SimulatorConfig::enable_trace`。

### `reset`

重置模拟器：寄存器清零、PC 归零、清除断点、清除流水线统计。

---

## 核心四件套（必做验收）

| 命令 | 功能 | 验收标准 |
|---|---|---|
| `step` / `s` | 单步执行 | 加载 ELF 后 PC 正确推进，失败有原因提示 |
| `continue` / `c` | 继续运行 | 遇断点暂停、exit 正常退出、状态正确显示 |
| `info r` / `regs` | 寄存器查看 | 32 个寄存器 + ABI 名 + PC 全部正确 |
| `x <addr>` | 内存查看 | hex dump 格式正确，`??` 标记越界，ASCII 列可读 |

---

## 接口约定（供 A 使用）

```cpp
// simulator.hpp 中已定义：
bool Simulator::step();
void Simulator::run(std::size_t max_steps = 0);
bool Simulator::reset();
uint32_t Simulator::pc() const;
uint32_t Simulator::reg(std::size_t index) const;  // 单个寄存器
const uint32_t* Simulator::regs() const;           // 寄存器数组
Memory& Simulator::memory();
bool Simulator::add_breakpoint(uint32_t addr);
bool Simulator::remove_breakpoint(uint32_t addr);
bool Simulator::has_breakpoint(uint32_t addr) const;
const vector<uint32_t>& Simulator::breakpoints() const;
void Simulator::set_trace_enabled(bool);
SimulatorStatus Simulator::status() const;
PipelineStats Simulator::pipeline_stats() const;
const PipelineSnapshot& Simulator::pipeline_snapshot() const;
```

---

## 错误处理

所有非法输入不崩溃，返回明确提示：

| 场景 | 返回消息 |
|---|---|
| 空命令 | 静默忽略 |
| 未知命令 | `unknown command` |
| `b` 缺参 | `b: missing address` |
| `b` 非法格式 | `b: invalid address` |
| `del` 缺参 | `del: missing index` |
| `del` 越界 | `del: out of range` |
| `x` 缺参 | `x: missing address — usage: ...` |
| `x` 非法地址 | `x: invalid address 'xyz' — expected hex` |
| `x` 写入越界 | `x: write failed at 0x... — address out of range` |
| `step` 非法步数 | `step: invalid count 'abc' — expected positive integer` |
| `step` 未加载程序 | `step: stopped at 1/1 — simulator is not ready` |
| `info` 非法子命令 | `info: unknown subcommand 'x' — use 'info r' or 'info b'` |
| `trace` 缺参 | `trace: missing argument` |
