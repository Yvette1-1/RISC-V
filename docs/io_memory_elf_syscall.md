# ELF / Memory / Syscall 模块说明

## 1. 职责范围

成员 B 负责程序装载链路和运行时 I/O 基础设施，主要包括：

- ELF32 RISC-V 文件解析与加载
- 字节寻址内存模型与权限检查
- 栈与堆初始化
- `write/read/exit/brk` syscall
- MMIO 与 UART 输出扩展接口预留

## 2. 内存布局

当前模拟器使用线性字节数组作为物理存储，并通过 `MemoryRegion` 描述虚拟地址范围：

| 区域 | 来源 | 权限 | 说明 |
|---|---|---|---|
| ELF 段 | `PT_LOAD` | 由 ELF flags 转换 | text/data/bss |
| heap | `brk` syscall | `MEM_READ | MEM_WRITE` | 从 ELF 最大段末尾 4KB 对齐位置开始 |
| stack | `SimulatorConfig::stack_top/stack_size` | `MEM_READ | MEM_WRITE` | `sp/x2` 初始化到 16 字节对齐栈顶 |
| MMIO | `SimulatorConfig::mmio_base/mmio_size` | `MEM_READ | MEM_WRITE` | 外设扩展预留 |

## 3. ELF 加载流程

`load_elf()` 执行以下步骤：

1. 打开 ELF 文件并检查文件大小。
2. 解析 ELF32 header。
3. 校验 magic、class、endian、version 和 machine。
4. 校验 Program Header 表范围。
5. 遍历 `PT_LOAD` 段：
   - 忽略 `mem_size == 0` 的空段。
   - 拒绝 `file_size > memory_size`。
   - 拒绝段文件范围越界。
   - 拒绝虚拟地址范围溢出或超出模拟器内存。
   - 拒绝加载段重叠。
   - 写入文件内容并对 BSS 区域清零。
6. 建立内存映射和访问权限。
7. 校验入口地址必须落在某个可加载段内。

## 4. syscall 行为

| syscall | 编号 | 行为 |
|---|---:|---|
| `read` | 63 | 仅支持 fd 0，读取宿主 stdin 到模拟器内存 |
| `write` | 64 | 支持 fd 1/2，从模拟器内存写到宿主 stdout/stderr |
| `exit` | 93 | 记录退出码并进入 `Exited` 状态 |
| `brk` | 214 | 查询或增长 heap break，不支持向后收缩 |

`brk` 增长时会映射新增 heap 区域并清零，且不会越过栈保护边界。

## 5. MMIO / UART 预留接口

`SimulatorConfig` 提供：

- `mmio_base`
- `mmio_size`
- `uart_tx_addr`

程序加载后会映射 MMIO 区域。store 指令路径会在普通内存写入失败时尝试 MMIO 写入：

- 写入 `uart_tx_addr` 的 1 字节数据会输出到宿主 stdout。
- 其他 MMIO 地址当前作为预留外设访问处理。

## 6. 冻结状态

Day10 后该模块进入功能冻结阶段。后续仅建议修复致命问题，不再主动增加新的 syscall 或复杂外设模型。
