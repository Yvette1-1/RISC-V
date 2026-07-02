# 调试器使用手册

## 概述

RISC-V 调试器是一个交互式命令行工具，用于控制模拟器执行、检查寄存器和内存状态、管理断点。

## 启动方式

```bash
# 无参数启动（空模拟器）
./build/riscv_sim

# 加载 ELF 程序
./build/riscv_sim examples/hello.elf
```

启动后进入 `RISC-V Debugger Shell` 交互提示符。

## 命令详解

### `help` / `h`

显示完整命令列表和用法示例。

### `quit` / `q`

退出调试器。

---

### `regs` / `info r`

显示全部 32 个通用寄存器（x0–x31）和 PC。

输出格式：`x<编号>(<ABI名>)=0x<值>`

```
PC  = 0x0001012c

x 0(zero)=0x00000000  x16(a6  )=0x00000000  
x 1(ra  )=0x00000000  x17(a7  )=0x00000000  
x 2(sp  )=0x80000000  x18(s2  )=0x00000000  
...
x15(a5  )=0x00000000  x31(t6  )=0x00000000  
```

`x0` 永远为 0（RISC-V 规范）。

---

### `si [N]` / `step [N]` / `s [N]`

单步执行指令。

- 不带参数：执行 1 条指令
- 带参数 `si 5`：连续执行 5 条，每步都检查返回状态

步进完成后自动打印寄存器状态。如果中途 `step()` 返回 `false`（例如 CPU 未实现该指令或停机），会报错并提前终止。

---

### `c` / `run`

继续执行，直到：

1. 遇到断点（CPU 执行循环调用 `has_breakpoint()`）
2. 程序正常退出（`exit` syscall）
3. 程序执行 `ebreak`
4. `run()` 内部停止条件触发

---

### `x <addr> [count]`

查看内存内容（hex dump）。

```
> x 0x10000 32
0x00010000  00 00 00 00 12 34 56 78 ff ff 00 00 00 00 00 00  |......4Vx........|
0x00010010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  |................|
```

- 地址自动对齐到 16 字节行边界
- `??` 表示该地址不可读（越界）
- 右侧显示可打印 ASCII 字符

### `x <addr>=<value>`

向内存地址写入 32 位值。

```
> x 0x20000=0xdeadbeef
Wrote 0xdeadbeef to 0x00020000
```

---

### `b <addr>` / `break <addr>`

在指定地址设置断点。

```
> b 0x1012c
Breakpoint #1 set at 0x0001012c
```

- 自动检查地址重复（同地址不会设两次）
- 断点 ID 自增，全局唯一

### `info b`

列出所有断点。

```
> info b
Breakpoints (2):
  [0] #1  0x0001012c  enabled
  [1] #2  0x00010140  enabled
```

- `[索引]` 用于 `del` 删除
- `#ID` 为全局唯一编号

### `del <n>` / `d <n>`

按索引删除断点。

```
> del 0
Deleted breakpoint #1 at 0x0001012c
```

---

### `trace on` / `trace off`

开启或关闭指令级执行跟踪。开启后，每次 `si` 都会打印每一步的 PC。

```
> trace on
Instruction trace: ON
> si 3
[info]  stepping 3 instruction(s)...
[trace] step[1/3] pc=0x00010130
[trace] step[2/3] pc=0x00010134
[trace] step[3/3] pc=0x00010138
```

---

### `reset`

重置模拟器状态：

- 寄存器清零、PC 归零
- 清除所有断点
- 关闭 trace
- 保留加载的程序（需重新 `load_program` 的话由上层决定）

---

## 错误提示说明

调试器对以下情况给出明确错误提示：

- 缺少必需参数：`x: missing argument — usage: x <addr> [count]`
- 无效地址格式：`x: invalid address 'xyz' — expected hex, e.g. x 0x1000`
- 断点索引越界：`del: breakpoint [999] out of range — 2 breakpoint(s) exist, valid range: [0, 1]`
- 未知命令：`unknown command: 'foo' — type 'help' for available commands`

## 接口约定（供 A 成员）

调试器提供以下公开方法供 CPU 执行循环调用：

```cpp
// 头文件: include/debugger.hpp

// 查询 PC 处是否有活跃断点
bool has_breakpoint(std::uint32_t pc) const;

// 查询是否开启指令跟踪
bool trace_enabled() const noexcept;
```

A 在 `Simulator::run()` 中的用法示例：

```cpp
void Simulator::run(std::size_t max_steps) {
    std::size_t count = 0;
    while (!halt_) {
        if (max_steps > 0 && count >= max_steps) break;
        // ★ 断点检查
        if (debugger_ && debugger_->has_breakpoint(pc_)) {
            halt_ = true;  // 或返回原因码
            break;
        }
        step();
        count++;
    }
}
```
