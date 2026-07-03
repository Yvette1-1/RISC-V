# 系统架构

## 1. 模块划分（5 类 14 个文件）

| 模块 | 文件数 | 负责人 | 职责 |
|---|---|---|---|
| CPU 核心 | `simulator.cpp`, `riscv_isa.hpp`, `isa.cpp` | A | 取指、译码、执行、写回、PC 更新 |
| 系统调用 | `simulator.cpp:handle_syscall()` | A | `write` / `read` / `exit` / `brk` |
| 内存系统 | `memory.cpp` | B | 虚拟内存映射、权限检查、8/16/32 位访存、批量读写 |
| ELF 加载器 | `elf_loader.cpp` | B | ELF32 解析、Program Header 遍历、段映射、入口地址 |
| 调试器 | `debugger.cpp` | C | 断点、单步、寄存器、内存、重置、`trace` |
| 主入口 | `main.cpp` | 框架 | 命令行参数解析、`Simulator` / `Debugger` 初始化 |
| 测试 | `tests/` 14 个 `.cpp` | A+B+C | 单元测试 + 程序级测试 + 边界测试 |
| 文档 | `docs/` 5 个 `.md` | C | 架构图、调试器手册、演示脚本、问题清单、回归清单 |

## 2. 体系结构图

```text
                        ┌──────────────────────┐
                        │     main.cpp         │
                        │  入口 / 参数解析      │
                        └──────┬───────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
    ┌─────────────┐   ┌─────────────┐   ┌──────────────┐
    │  Debugger   │   │  Simulator  │   │  ELF Loader  │
    │  (C)        │◄──│  (A)        │──▶│  (B)         │
    │  repl()     │   │  step()     │   │  load_elf()  │
    │  handle_    │   │  run()      │   │  parse/load  │
    │   command() │   │  reset()    │   │  segments    │
    └──────┬──────┘   └──────┬──────┘   └──────┬───────┘
           │                 │                  │
           │         ┌───────┼───────┐          │
           │         ▼       ▼       ▼          │
           │  ┌──────────┐ ┌──────────┐ ┌──────▼───────┐
           │  │riscv_isa │ │ Memory   │ │  ElfLoad     │
           │  │decode()  │ │ (B)      │ │  Result      │
           │  │get_bits()│ │map/load/ │ │  segments    │
           │  │sign_ext  │ │store     │ │  entry_point │
           │  └──────────┘ └──────────┘ └──────────────┘
           │
           ▼
    ┌──────────────┐
    │  用户输入     │
    │  stdin        │
    └──────────────┘
```

## 3. 执行流程图

```text
  ELF 文件 ──▶ load_elf() ──▶ 解析 ELF Header
                                    │
                            遍历 Program Header
                                    │
                            映射 .text/.data 到 Memory
                                    │
                            返回 entry_point ──▶ pc = entry
                                                       │
  ┌────────────────────────────────────────────────────┘
  ▼
  run() 执行循环:
  ┌─────────────────────────────────────────┐
  │  state == Running/Loaded?               │──── 否 ──▶ 退出
  │      是                                 │
  │  has_breakpoint(pc)?                    │──── 是 ──▶ Paused ──▶ 返回调试器
  │      否                                 │
  │  fetch(pc) ──▶ decode(inst)             │
  │       │                                 │
  │       ├─ ALU 指令 ──▶ 寄存器写回         │
  │       ├─ Load/Store ──▶ Memory 读写     │
  │       ├─ Branch/Jump ──▶ 更新 PC        │
  │       ├─ ecall ──▶ handle_syscall()     │
  │       │    ├─ write ──▶ stdout          │
  │       │    ├─ read  ──▶ stdin           │
  │       │    ├─ exit  ──▶ Exited          │
  │       │    └─ brk   ──▶ 调整堆          │
  │       └─ ebreak ──▶ Halted              │
  │       │                                 │
  │  pc = next_pc                           │
  │  executed++                             │
  └─────────────────────────────────────────┘
```

## 4. 虚拟内存布局

```text
  ┌─────────────────┐ 0x00000000
  │                 │
  │  未映射区域      │
  │                 │
  ├─────────────────┤ text_vaddr (ELF PT_LOAD)
  │  .text (代码段)  │ ← R+X
  ├─────────────────┤
  │  .rodata (只读)  │ ← R
  ├─────────────────┤
  │  .data (数据段)  │ ← R+W
  ├─────────────────┤
  │  .bss (零初始化) │ ← R+W（file_size → memory_size 填零）
  ├─────────────────┤ heap_base (~0x7ffe0000 - 1M)
  │                 │
  │  Heap (堆)      │ ← brk() 向上增长
  │                 │
  ├─────────────────┤ stack_top (0x7fff0000)
  │                 │
  │  Stack (栈)     │ ← 向下增长（1MB）
  │                 │
  └─────────────────┘ memory_size (64MB default)
```

## 5. 调试器命令交互流程

```text
  用户输入 "step"
       │
       ▼
  repl() ──▶ handle_command("step")
       │
       ├─ tokenize → cmd="step"
       ├─ 调用 simulator_.step()
       ├─ 输出新 PC / 状态
       └─ 返回 {handled=true, should_exit=false, message=""}
```

## 6. 冻结接口

| 接口 | 提供方 | 消费方 |
|---|---|---|
| `Simulator::step()` | A | C（`step` 命令） |
| `Simulator::run()` | A | C（`continue` 命令） |
| `Simulator::pc() / regs()` | A | C（`regs` 命令） |
| `Simulator::memory()` | B | C（`x` 命令） |
| `Simulator::add/remove/has_breakpoint()` | A | C（`b` / `del` 命令） |
| `Simulator::set_trace_enabled()` | A | C（`trace` 命令） |
| `Simulator::status()` | A | C（状态显示） |
| `load_elf()` | B | A（`Simulator::load_program`） |
| `Memory::load8/16/32 / store8/16/32` | B | A + C |
| `Debugger::handle_command()` | C | `main.cpp` |

## 7. 指令解码表

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

## 8. 五级流水线状态转换图

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

## 9. CPI 统计分析

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

## 10. GDB RSP 协议支持范围

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

## 11. 验收标准映射

| 验收标准 | 对应实现/材料 |
|---|---|
| riscv-tests 通过率 ≥ 95% | `tests/riscv-tests/isa/rv32ui` 已可成功编译；本地 CTest 回归用于模拟器功能验证 |
| 调试器可对 Hello World 设断点、单步、打印寄存器 | `Debugger` 支持 `b`、`continue`、`step`、`regs` |
| 提交指令解码表 | 本文第 7 节与 `kDecodeTable` |
| 提交流水线状态转换图 | 本文第 8 节 |
| 提交 CPI 统计分析 | 本文第 9 节 |
| GDB RSP 协议 | `GdbRspSession` 与 `riscv_gdb_rsp_test` |
