# 回归测试清单

> 更新日期：2026-07-08

## 调试器命令（28 项）

| # | 验证项 | 操作 | 状态 |
|---|---|---|---|
| 1 | 启动 | `./build/riscv_sim` | ✅ |
| 2 | help | `help` | ✅ |
| 3 | quit | `quit` | ✅ |
| 4 | regs | `regs` | ✅ |
| 5 | info r | `info r` | ✅ |
| 6 | info b（空） | `info b` | ✅ |
| 7 | step | `step` | ✅ |
| 8 | step N | `step 5` | ✅ |
| 9 | c | `c` | ✅ |
| 10 | x addr | `x 0x0 16` | ✅ |
| 11 | x addr=val | `x 0x100=0xdead` | ✅ |
| 12 | x 确认写 | `x 0x100 4` | ✅ |
| 13 | x 越界 | `x 0xfffffff0` | ✅ |
| 14 | b addr | `b 0x1012c` | ✅ |
| 15 | b 重复 | 两次 `b 0x1012c` | ✅ |
| 16 | info b | `info b` | ✅ |
| 17 | del | `del 0` | ✅ |
| 18 | del 越界 | `del 999` | ✅ |
| 19 | trace on/off | `trace on`/`trace off` | ✅ |
| 20 | reset | `reset` | ✅ |
| 21 | 未知命令 | `xyzzy` | ✅ |
| 22 | b 缺参 | `b` | ✅ |
| 23 | b 非法 | `b xyz` | ✅ |
| 24 | x 缺参 | `x` | ✅ |
| 25 | x 非法 | `x xyz` | ✅ |
| 26 | del 缺参 | `del` | ✅ |
| 27 | info 非法 | `info x` | ✅ |
| 28 | **demo** | `demo` | ✅ |

## 自动化测试

| # | 测试 | 状态 |
|---|---|---|
| 29 | `riscv_debugger_test` | ✅ |
| 30 | `riscv_debugger_edges_test` | ✅ |
| 31 | `riscv_breakpoint_test` | ✅ |
| 32 | `riscv_cpu_test` | ✅ |
| 33 | `riscv_memory_test` | ✅ |
| 34 | `riscv_hello_test` | ✅ |
| 35 | `riscv_loop_test` | ✅ |
| 36 | `riscv_recursion_test` | ✅ |
| 37 | `riscv_array_test` | ✅ |
| 38 | `riscv_syscall_edges_test` | ✅ |
| 39 | `riscv_elf_loader_edges_test` | ✅ |
| 40 | `riscv_illegal_exit_test` | ✅ |

