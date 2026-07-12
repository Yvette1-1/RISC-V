# RISC-V Execution Studio

RISC-V Execution Studio 是一款面向 RISC-V 模拟器的高级可视化工作台。它集成了模拟器控制、可视化调试、微架构性能分析以及远程调试控制台功能。

## 🎯 核心目标

它打破了传统纯文本终端调试的局限，将冰冷的机器状态转化为极具洞察力的高级语义图表：
1. **看得懂程序在跑什么**：实时高亮执行流，支持反汇编与断点联动。
2. **看得见 CPU 内部状态怎么变**：提供寄存器变更动效（Flash）与 CPU 周期指标监控。
3. **看得出性能瓶颈在哪里**：五级流水线可视化与内存访问热图。
4. **看得出系统行为过程**：Syscall 调用过程记录与终端输出。

## 🚀 快速开始

本项目使用 React + TypeScript + Vite + Tailwind CSS 构建。

### 1. 安装依赖

确保你已经安装了 Node.js，推荐使用 `pnpm`：

```bash
pnpm install
```

### 2. 启动开发服务器

```bash
pnpm run dev
```

运行后，浏览器将自动打开（或手动访问 `http://localhost:5173`）以预览此交互界面。

## 🏗 界面模块与交互说明

### 1. 顶部工具栏 (Toolbar)
- 负责程序的全局运行控制。
- **Run/Pause**：切换执行状态。
- **Step**：单步执行（此时可观察右侧寄存器的高亮变化）。
- **Reset/Stop**：重置状态。

### 2. 左侧边栏 (Left Sidebar)
- **ELF Sections**：展示程序的各个段及其基地址。
- **Breakpoints**：与中央反汇编视图联动的断点管理器，支持点击删除断点。
- **Syscall History**：记录程序触发的系统调用（如 write, brk 等）。

### 3. 中央工作区 (Main Workspace)
多 Tab 设计，负责核心数据的深度可视化：
- **Disassembly（反汇编）**：展示指令流，高亮当前 PC（带有脉冲动画），点击指令行即可设置/取消断点。
- **Pipeline（流水线）**：展示 IF/ID/EX/MEM/WB 五级流水线状态，直观标识正常（绿色）、停顿（Stall，琥珀色）、清空（Flush，紫色）等状态。
- **Memory Heatmap（内存热图）**：根据读写频率对不同内存区域（Text, Data, BSS, Heap, Stack）进行色块深浅展示，方便寻找访问热点。

### 4. 右侧边栏 (Right Sidebar)
- **CPU State**：仪表盘，实时显示 PC 指针、执行周期（Cycles）、CPI、执行指令数等关键性能指标。
- **General Registers**：32 个通用寄存器网格视图。当值发生变化时（可通过点击 Toolbar 的 Step 模拟），对应的寄存器会短暂闪烁发光，提示副作用。

### 5. 底部控制台 (Bottom Console)
- **Console & Logs**：统一的日志输出窗口，包含系统日志、系统调用（Syscall）、GDB 协议状态和错误信息，支持清空操作。

## 🛠 技术栈

- **React 18**
- **TypeScript**
- **Vite**
- **Tailwind CSS** (用于极速 UI 样式构建)
- **Zustand** (轻量级全局状态管理，位于 `src/store/useStore.ts`)
- **Lucide React** (现代 SVG 图标库)

## 🔗 后续接入指南

当前的 UI 层使用了 Zustand (`src/store/useStore.ts`) 作为状态中心并填充了模拟数据。
要接入真实的 C++ RISC-V 模拟器，你只需要修改 `useStore.ts` 中的交互逻辑：

1. **通过 WebAssembly**：将 C++ 引擎编译为 Wasm 并在浏览器中实例化，直接调用其 Step/Run 接口并读取状态更新到 Store。
2. **通过 WebSocket API**：让 C++ 模拟器作为本地服务端运行，UI 前端通过 WebSocket 监听 CPU 状态的广播并发送控制指令。