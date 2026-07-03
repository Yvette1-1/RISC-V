# 架构与验收材料

## 1. 模块划分

- `Simulator`：统一调度执行、重置、装载程序，并维护 PC、寄存器、断点、流水线快照与 CPI 统计。
- `Memory`：字节寻址内存与边界检查。
- `load_elf()`：负责 ELF 程序映射、入口地址解析、`.tohost`/`.fromhost` 符号定位。
- `decode()` / `kDecodeTable`：负责 RV32 指令字段拆分与指令解码表描述。
- `Debugger`：命令行调试控制，支持断点、单步、继续运行、寄存器打印、流水线/CPI 查看。
- `GdbRspSession`：GDB Remote Serial Protocol 的包编解码与核心调试命令处理。

## 2. 指令解码表

代码中的权威解码表位于 `include/riscv_isa.hpp` 的 `kDecodeTable`。当前覆盖基础 RV32I 常用整数指令：

| 指令 | opcode | funct3 | funct7 | 格式 | 语义 |
|---|---:|---:|---:|---|---|
| LUI | 0x37 | * | * | U | `rd = imm20 << 12` |
| AUIPC | 0x17 | * | * | U | `rd = pc + (imm20 << 12)` |
| JAL | 0x6f | * | * | J | `rd = pc + 4; pc += offset` |
| JALR | 0x67 | 0x0 | * | I | `rd = pc + 4; pc = (rs1 + imm) & ~1` |
| BEQ/BNE/BLT/BGE/BLTU/BGEU | 0x63 | 0x0/1/4/5/6/7 | * | B | 条件分支 |
| LB/LH/LW/LBU/LHU | 0x03 | 0x0/1/2/4/5 | * | I | 加载 |
| SB/SH/SW | 0x23 | 0x0/1/2 | * | S | 存储 |
| ADDI/SLTI/SLTIU/XORI/ORI/ANDI | 0x13 | 0x0/2/3/4/6/7 | * | I | 立即数算术/逻辑 |
| SLLI/SRLI/SRAI | 0x13 | 0x1/5/5 | 0x00/0x00/0x20 | I | 立即数移位 |
| ADD/SUB/SLL/SLT/SLTU/XOR/SRL/SRA/OR/AND | 0x33 | 对应 funct3 | 0x00 或 0x20 | R | 寄存器算术/逻辑 |
| FENCE | 0x0f | 0x0 | * | I | 内存序约束 |
| ECALL/EBREAK | 0x73 | 0x0 | 0x00/0x01 | I | 系统调用/断点 |

字段拆分使用位操作：

- `opcode = inst[6:0]`
- `rd = inst[11:7]`
- `funct3 = inst[14:12]`
- `rs1 = inst[19:15]`
- `rs2 = inst[24:20]`
- `funct7 = inst[31:25]`

## 3. 五级流水线状态转换图

当前模拟器仍以顺序解释执行保证功能正确性，同时维护五级流水线抽象快照用于建模与统计：IF、ID、EX、MEM、WB。

```text
          +------+    +------+    +------+    +------+    +------+
fetch --> |  IF  | -> |  ID  | -> |  EX  | -> | MEM  | -> |  WB  | --> retire
          +------+    +------+    +------+    +------+    +------+
             ^
             |
             +--- next PC
```

每条成功退休的指令会推动一次流水线寄存器迁移：

```text
WB  <- MEM
MEM <- EX
EX  <- ID
ID  <- IF
IF  <- next PC
```

控制流改变时，例如分支、跳转、异常入口，模型记录一次 `flush`，并清空 IF/ID 的有效位，表示前端取到的顺序路径指令被冲刷。

流水线快照字段：

| 字段 | 含义 |
|---|---|
| `cycle` | 当前抽象周期 |
| `if_pc/id_pc/ex_pc/mem_pc/wb_pc` | 各级对应 PC |
| `if_valid/id_valid/ex_valid/mem_valid/wb_valid` | 各级是否有效 |

## 4. CPI 统计分析

统计结构为 `PipelineStats`：

| 字段 | 含义 |
|---|---|
| `cycles` | 抽象流水线周期数 |
| `instructions` | 成功退休的指令数 |
| `stalls` | 数据/结构冒险停顿数，当前顺序模型中默认为 0 |
| `flushes` | 控制流改变导致的冲刷次数 |
| `cpi` | `cycles / instructions` |

当前顺序解释器每退休一条指令推进一个抽象流水线周期，因此无冒险时 CPI 接近 1.0。分支、跳转或异常导致的控制改变会增加 `flushes`，用于分析控制冒险频率。可以在调试器中执行：

```text
pipeline
```

输出示例：

```text
cycles=10 instructions=10 stalls=0 flushes=2 CPI=1.000
IF  0x80000028
ID  --
EX  0x80000020
MEM 0x8000001c
WB  0x80000018
```

## 5. GDB RSP 协议支持范围

`GdbRspSession` 支持以下基础远程调试命令：

| RSP 包 | 功能 |
|---|---|
| `?` | 返回停止原因 `S05` |
| `g` | 读取 32 个通用寄存器和 PC |
| `mADDR,LEN` | 读取内存 |
| `MADDR,LEN:HEX` | 写内存 |
| `Z0,ADDR,KIND` | 设置软件断点 |
| `z0,ADDR,KIND` | 删除软件断点 |
| `s` | 单步 |
| `c` | 继续运行 |
| `qSupported` | 返回基础能力 |
| `D` / `k` | 断开/终止 |

该实现提供协议包校验和编解码，可作为 socket 监听器或 IDE 调试前端的后端会话核心。

## 6. 验收标准映射

| 验收标准 | 对应实现/材料 |
|---|---|
| riscv-tests 通过率 ≥ 95% | `tests/riscv-tests/isa/rv32ui` 已可成功编译；本地 CTest 回归用于模拟器功能验证 |
| 调试器可对 Hello World 设断点、单步、打印寄存器 | `Debugger` 支持 `b`、`continue`、`step`、`regs` |
| 提交指令解码表 | 本文第 2 节与 `kDecodeTable` |
| 提交流水线状态转换图 | 本文第 3 节 |
| 提交 CPI 统计分析 | 本文第 4 节 |
| GDB RSP 协议 | `GdbRspSession` 与 `riscv_gdb_rsp_test` |
