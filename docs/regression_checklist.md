# 回归测试清单

> 每次代码变更后逐项验证。更新日期：2026-07-07

## 调试器命令回归（C 负责，28 项）

| # | 验证项 | 操作 | 预期结果 | 状态 |
|---|---|---|---|---|
| 1 | 启动 | `./build/riscv_sim` | 显示主菜单 | ✅ |
| 2 | help | `help` | 显示 15 个命令 | ✅ |
| 3 | quit | `quit` | 返回主菜单 | ✅ |
| 4 | regs | `regs` | 32 寄存器 + ABI 名 + PC | ✅ |
| 5 | info r | `info r` | 同 regs | ✅ |
| 6 | info b（空） | `info b` | No breakpoints. | ✅ |
| 7 | step | `step` | 单步，失败有提示 | ✅ |
| 8 | step N | `step 5` | 单步 5 次 | ✅ |
| 9 | c | `c` | 继续执行至停止 | ✅ |
| 10 | x addr | `x 0x0 16` | hex dump + ASCII | ✅ |
| 11 | x addr=val | `x 0x100=0xdead` | Wrote 提示 | ✅ |
| 12 | x 确认写 | `x 0x100 4` | ef be ad de | ✅ |
| 13 | x 越界 | `x 0xfffffff0` | ?? 标记 | ✅ |
| 14 | b addr | `b 0x1012c` | Breakpoint set | ✅ |
| 15 | b 重复 | 两次 `b 0x1012c` | 拒绝重复 | ✅ |
| 16 | info b | `info b` | 断点列表 | ✅ |
| 17 | del | `del 0` | Deleted breakpoint | ✅ |
| 18 | del 越界 | `del 999` | del: out of range | ✅ |
| 19 | trace on/off | `trace on`/`trace off` | ON/OFF | ✅ |
| 20 | reset | `reset` | Reset complete | ✅ |
| 21 | 未知命令 | `xyzzy` | unknown command | ✅ |
| 22 | b 缺参 | `b` | b: missing address | ✅ |
| 23 | b 非法 | `b xyz` | b: invalid address | ✅ |
| 24 | x 缺参 | `x` | x: missing address | ✅ |
| 25 | x 非法 | `x xyz` | x: invalid address | ✅ |
| 26 | del 缺参 | `del` | del: missing index | ✅ |
| 27 | info 非法子命令 | `info x` | info: unknown subcommand | ✅ |
| 28 | **demo** | `demo`（需先加载 ELF） | 6 步流程全部执行 | ✅ |

## 断点功能回归（6 项）

| # | 验证项 | 操作 | 状态 |
|---|---|---|---|
| 29 | 添加断点 | `sim.add_breakpoint(0x1000)` | ✅ |
| 30 | 查询断点 | `sim.has_breakpoint(0x1000)` | ✅ |
| 31 | 重复添加 | 再次 add → false | ✅ |
| 32 | 删除断点 | `sim.remove_breakpoint(0x1000)` | ✅ |
| 33 | 查询已删 | `sim.has_breakpoint(0x1000)` → false | ✅ |
| 34 | 删除不存在 | 再次 remove → false | ✅ |

## CPU 指令回归（A 负责）

| # | 测试 | 状态 |
|---|---|---|
| 35 | `riscv_cpu_test` | ✅ |
| 36 | `riscv_syscall_test` | ✅ |
| 37 | `riscv_syscall_edges_test` | ✅ |

## 程序级回归（A+B 负责）

| # | 测试 | 状态 |
|---|---|---|
| 38 | `riscv_hello_test` | ✅ |
| 39 | `riscv_loop_test` | ✅ |
| 40 | `riscv_recursion_test` | ✅ |
| 41 | `riscv_array_test` | ✅ |

## 内存/ELF 回归（B 负责）

| # | 测试 | 状态 |
|---|---|---|
| 42 | `riscv_memory_test` | ✅ |
| 43 | `riscv_elf_loader_edges_test` | ✅ 已修复 |
| 44 | `riscv_illegal_exit_test` | ✅ 已修复 |

## riscv-tests 套件（C 搭建，A 指令支撑）

| # | 类别 | 通过/总计 | 通过率 |
|---|---|---|---|
| 45 | 算术/移位（add, sub, slli, srli, srai, lui, auipc） | 8/8 | 100% |
| 46 | 分支/跳转（beq, bne, blt, bge, bltu, bgeu, jal, jalr） | 8/8 | 100% |
| 47 | 访存（lw, sw, lh, sh, lb, sb） | 2/2 | 100% |
| 48 | M 扩展（mul, div, rem, mulh, etc） | 6/6 | 100% |
| 49 | 位运算（and, or, xor, andi, ori, xori） | 0/1 | 0%（A 未实现） |
| 50 | 比较（slt, sltu, slti, sltiu） | 0/1 | 0%（A 未实现） |
| **总计** | | **22/24** | **91.6%** |

## 兼容性

| # | 项 | 状态 |
|---|---|---|
| 51 | `test_smoke.cpp`（GCC 8.1 <filesystem>） | ⚠️ 跳过 |

## 汇总

| 类别 | 通过/总计 | 通过率 |
|---|---|---|
| 调试器命令 | 28/28 | 100% |
| 断点功能 | 6/6 | 100% |
| CPU 指令 | 3/3 | 100% |
| 程序级 | 4/4 | 100% |
| 内存/ELF | 4/4 | 100% |
| riscv-tests | 22/24 | 91.6% |
| **C 负责项** | **34/34** | **100%** |
| **全项目** | **67/69** | **97.1%** |
