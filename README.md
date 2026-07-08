# RISC-V 模拟器与调试器

面向 RISC-V 的指令集模拟器与交互式调试器，支持 RV32I + M 扩展、ELF32 程序加载、虚拟内存映射、系统调用等。

## 目录结构

```
├── include/       公共头文件
├── src/           核心源码
├── tests/         测试程序
├── docs/          设计文档
├── examples/      示例程序
```

## 编译

```bash
cmake -S . -B build
cmake --build build
```

## 运行

```bash
build\riscv_sim.exe                # 启动主菜单
#1. 装载并运行 ELF 程序
#2. 进入调试器
#3. 查看 GDB 远程调试状态
#4. 查看快速示例
#5. 退出
```

## 调试器命令

进入调试 Shell（菜单选 2）后可用：

| 命令 | 说明 |
|---|---|
| `help` | 命令列表 |
| `regs` / `info r` | 32 寄存器 + PC |
| `step [N]` / `s [N]` | 单步 N 条 |
| `continue` / `c` | 继续运行 |
| `demo` | 一键演示（断点→寄存器→单步→内存） |
| `pipeline` | 五级流水线 + CPI 统计 |
| `x <addr> [count]` | 内存查看 |
| `x <addr>=<value>` | 内存写入 |
| `b <addr>` | 设断点 |
| `info b` | 断点列表 |
| `del <n>` | 删断点 |
| `trace on/off` | 指令跟踪 |
| `reset` | 重置 |
| `quit` | 退出 |

