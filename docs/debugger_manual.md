# 调试器使用手册

## 启动

```
./build/riscv_sim                  # 主菜单
→ 选 1 加载 ELF → 选 2 进入调试 Shell
```

## 全部命令

| 命令 | 说明 |
|---|---|
| `help` / `h` | 命令列表 |
| `quit` / `q` | 退出 Shell |
| `regs` / `r` | 32 寄存器 + ABI 名 + PC |
| `info r` | 同 regs |
| `info b` | 断点列表 |
| `step [N]` / `s [N]` | 单步 N 条 |
| `continue` / `c` | 继续运行 |
| `demo` | 一键演示（断点→寄存器→单步→内存→继续） |
| `pipeline` / `cpi` | 流水线 + CPI 统计 |
| `x <addr> [count]` | 内存查看（hex dump），默认 16 字节 |
| `x <addr>=<value>` | 内存写入（小端序） |
| `b <addr>` / `break` | 设断点 |
| `del <n>` / `d` | 删断点 |
| `trace on/off` | 指令跟踪 |
| `reset` | 重置 |

## 核心四件套

| 命令 | 功能 |
|---|---|
| `step` | 单步执行 |
| `c` | 继续运行 |
| `info r` | 寄存器查看 |
| `x <addr>` | 内存查看 |

