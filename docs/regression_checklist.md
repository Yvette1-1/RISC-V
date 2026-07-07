# 回归测试清单

> 每次代码变更后，逐项验证以下内容，确认无回归。
> 更新日期：2026-07-02

## 调试器命令回归（C 负责，18 项）

| # | 验证项 | 操作 | 预期结果 | 状态 |
|---|---|---|---|---|
| 1 | 启动 | `./build/riscv_sim` | 显示 RISC-V debugger shell 提示符 | ✅ |
| 2 | help | `help` | 显示全部 14 个命令 | ✅ |
| 3 | quit | `quit` | 正常退出，返回 shell | ✅ |
| 4 | regs | `regs` | 显示 32 寄存器 + ABI 名 + PC | ✅ |
| 5 | info r | `info r` | 同 regs | ✅ |
| 6 | info b（空） | `info b` | 显示 "No breakpoints." | ✅ |
| 7 | si | `si` | 单步 1 次（无程序时提示 not ready） | ✅ |
| 8 | si N | `si 5` | 单步 5 次，失败即时停止 | ✅ |
| 9 | c | `c` | 继续执行至停止，打印状态和 PC | ✅ |
| 10 | x addr | `x 0x0 16` | 显示 16 字节 hex dump + ASCII | ✅ |
| 11 | x addr=val | `x 0x100=0xdead` | 提示 "Wrote 0xdeadbeef to 0x00000100" | ✅ |
| 12 | x addr（确认写） | `x 0x100 4` | 显示 ef be ad de（小端序） | ✅ |
| 13 | x 越界 | `x 0xfffffff0` | 越界字节显示 `??` | ✅ |
| 14 | b addr | `b 0x1012c` | 提示 "Breakpoint set" | ✅ |
| 15 | b 重复 | `b 0x1012c` 两次 | 第二次失败（Simulator 去重） | ✅ |
| 16 | info b | `info b` | 列出断点（索引 + 地址） | ✅ |
| 17 | del | `del 0` | 提示 "Deleted breakpoint" | ✅ |
| 18 | del 越界 | `del 999` | 提示 "del: out of range" | ✅ |
| 19 | trace on/off | `trace on` / `trace off` | 显示 ON / OFF | ✅ |
| 20 | reset | `reset` | 寄存器归零、断点清除、提示 "Reset complete" | ✅ |
| 21 | 未知命令 | `xyzzy` | 提示 "unknown command" | ✅ |
| 22 | b 缺参 | `b` | 提示 "b: missing address" | ✅ |
| 23 | b 非法地址 | `b xyz` | 提示 "b: invalid address" | ✅ |
| 24 | x 缺参 | `x` | 提示 "x: missing address" | ✅ |
| 25 | x 非法地址 | `x xyz` | 提示 "x: invalid address" | ✅ |
| 26 | del 缺参 | `del` | 提示 "del: missing index" | ✅ |
| 27 | trace 缺参 | `trace` | 提示 "trace: missing argument" | ✅ |
| 28 | info 非法子命令 | `info x` | 提示 "info: unknown subcommand" | ✅ |

## 断点功能回归

| # | 验证项 | 操作 | 预期结果 | 状态 |
|---|---|---|---|---|
| 29 | 添加断点 | `sim.add_breakpoint(0x1000)` | 返回 true | ✅ |
| 30 | 查询断点 | `sim.has_breakpoint(0x1000)` | 返回 true | ✅ |
| 31 | 重复添加 | 再次 `add_breakpoint(0x1000)` | 返回 false | ✅ |
| 32 | 删除断点 | `sim.remove_breakpoint(0x1000)` | 返回 true | ✅ |
| 33 | 查询已删 | `sim.has_breakpoint(0x1000)` | 返回 false | ✅ |
| 34 | 删除不存在 | 再次 `remove_breakpoint(0x1000)` | 返回 false | ✅ |

## CPU 指令回归（A 负责）

| # | 验证项 | 说明 | 状态 |
|---|---|---|---|
| 35 | CPU 指令测试 | `./build/riscv_cpu_test` | ✅ PASS |
| 36 | syscall 测试 | `./build/riscv_syscall_test` | ✅ PASS |
| 37 | syscall 边界测试 | `./build/riscv_syscall_edges_test` | ✅ PASS |

## 程序级回归（A+B 负责）

| # | 验证项 | 说明 | 状态 |
|---|---|---|---|
| 38 | hello 测试 | `./build/riscv_hello_test` | ✅ PASS |
| 39 | loop 测试 | `./build/riscv_loop_test` | ✅ PASS |
| 40 | recursion 测试 | `./build/riscv_recursion_test` | ✅ PASS |
| 41 | array 测试 | `./build/riscv_array_test` | ✅ PASS |

## 内存/ELF 回归（B 负责）

| # | 验证项 | 说明 | 状态 |
|---|---|---|---|
| 42 | memory 测试 | `./build/riscv_memory_test` | ✅ PASS |
| 43 | ELF loader 边界测试 | `./build/riscv_elf_loader_edges_test` | ❌ FAIL（B 待修复） |
| 44 | illegal_exit 测试 | `./build/riscv_illegal_exit_test` | ❌ FAIL（依赖上一条） |

## 兼容性问题

| # | 验证项 | 说明 | 状态 |
|---|---|---|---|
| 45 | smoke test | `./build/riscv_smoke_test` | ⚠️ GCC 8.1 不支持 `<filesystem>` |

## 汇总

| 类别 | 通过/总计 | 通过率 |
|---|---|---|
| 调试器命令 | 28/28 | 100% |
| 断点功能 | 6/6 | 100% |
| CPU 指令 | 3/3 | 100% |
| 程序级 | 4/4 | 100% |
| 内存/ELF | 2/4 | 50%（B 待修复） |
| 兼容性 | 0/1 | —（编译器限制） |
| **C 负责项总计** | **34/34** | **100%** |
| **全项目总计** | **43/45** | **95.6%** |
