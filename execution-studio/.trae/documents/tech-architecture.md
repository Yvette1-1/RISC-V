# RISC-V Execution Studio 技术架构文档

## 1. 架构设计

```mermaid
graph TD
    subgraph "前端应用层 (Frontend)"
        A["React 18 主视图"]
        B["Zustand 状态管理"]
        C["Tailwind CSS 样式"]
        D["Framer Motion 动画"]
    end
    
    subgraph "核心逻辑层 (Core Logic)"
        E["CPU 状态 Mock/Bridge"]
        F["指令解析与格式化"]
        G["流水线状态推演器"]
    end
    
    subgraph "后端/模拟器层 (Backend/Simulator)"
        H["真实的 RISC-V 模拟器 (C/C++/Rust)"]
        I["WebSocket/HTTP 通信"]
    end

    A <--> B
    B <--> E
    E <--> F
    E <--> G
    A --> C
    A --> D
    E <..> I
    I <..> H
```
*注：目前阶段主要聚焦于前端 UI 及其模拟状态展示，后续将通过 WebSocket 或 REST API 连接到真实的后端模拟器。*

## 2. 技术栈描述
- **前端框架**: React@18 (组件化开发)
- **构建工具**: Vite (快速热更新与打包)
- **样式方案**: Tailwind CSS@3 (原子化 CSS，便于实现深色专业主题) + `clsx`/`tailwind-merge` (动态类名)
- **状态管理**: Zustand (轻量级，适合频繁更新的 CPU 寄存器与内存状态)
- **图标库**: Lucide React (简洁的线性图标，适合专业工具)
- **动画库**: Framer Motion (用于寄存器发光、流水线闪烁、高亮滚动等微交互动效)
- **数据可视化**: Recharts (用于 CPI、性能统计等图表展示)

## 3. 路由定义
作为单页应用 (SPA) 工作台，界面采用组件面板切换，无需复杂路由。
| 路由 | 用途 |
|-------|---------|
| `/` | 工作台主页面 (RISC-V Execution Studio) |

## 4. 状态模型 (Zustand Store 规划)

### 4.1 全局状态
```typescript
interface ExecutionState {
  // 控制状态
  isRunning: boolean;
  isPaused: boolean;
  isGdbAttached: boolean;
  
  // CPU 状态
  pc: number;
  registers: Record<string, number>; // x0-x31
  previousRegisters: Record<string, number>; // 记录上一次状态以实现发光动效
  csr: Record<string, number>;
  
  // 内存与代码
  instructions: Instruction[];
  breakpoints: number[];
  
  // 流水线状态
  pipeline: {
    IF: PipelineStage;
    ID: PipelineStage;
    EX: PipelineStage;
    MEM: PipelineStage;
    WB: PipelineStage;
  };
  
  // 性能统计
  stats: {
    cycles: number;
    instructions: number;
    cpi: number;
    stalls: number;
    flushes: number;
  };
  
  // 日志输出
  logs: LogEntry[];
}
```

## 5. UI 组件层级
- `App`: 根组件，挂载全局状态。
- `Layout`: 包含 Dock 布局的核心容器。
  - `Toolbar`: 顶部控制按钮组。
  - `Sidebar`: 左侧 ELF 和断点导航。
  - `Workspace`: 中央主区域。
    - `DisassemblyView`: 反汇编视图，带滚动联动。
    - `PipelineView`: 五级流水线动画展示。
    - `MemoryHeatmap`: 内存区域分布图。
  - `CpuStatePanel`: 右侧寄存器和状态监控盘。
  - `ConsolePanel`: 底部输出窗口。
