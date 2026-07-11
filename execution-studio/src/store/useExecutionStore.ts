import { create } from 'zustand';

export interface Instruction {
  address: number;
  hex: string;
  asm: string;
  type?: string;
  sourceLine?: number;
}

export interface PipelineStage {
  inst: Instruction | null;
  status: 'valid' | 'stall' | 'flush' | 'empty';
}

export interface LogEntry {
  id: string;
  timestamp: number;
  type: 'info' | 'error' | 'syscall' | 'gdb';
  message: string;
}

interface ExecutionState {
  // 控制状态
  isRunning: boolean;
  isPaused: boolean;
  isGdbAttached: boolean;
  
  // CPU 状态
  pc: number;
  registers: Record<string, number>;
  previousRegisters: Record<string, number>;
  csr: Record<string, number>;
  
  // 内存与代码
  instructions: Instruction[];
  breakpoints: number[];
  activeTab: 'disassembly' | 'source' | 'pipeline' | 'memory' | 'history';
  
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
    instCount: number;
    cpi: number;
    stalls: number;
    flushes: number;
  };
  
  // 日志输出
  logs: LogEntry[];

  // Actions
  toggleRun: () => void;
  step: () => void;
  reset: () => void;
  toggleBreakpoint: (address: number) => void;
  setActiveTab: (tab: ExecutionState['activeTab']) => void;
  addLog: (type: LogEntry['type'], message: string) => void;
}

// 模拟一些初始指令
const mockInstructions: Instruction[] = [
  { address: 0x10000, hex: '00000093', asm: 'addi x1, x0, 0', type: 'I' },
  { address: 0x10004, hex: '00100113', asm: 'addi x2, x0, 1', type: 'I' },
  { address: 0x10008, hex: '002081b3', asm: 'add x3, x1, x2', type: 'R' },
  { address: 0x1000c, hex: '00000073', asm: 'ecall', type: 'I' },
  { address: 0x10010, hex: '00008067', asm: 'ret', type: 'I' },
];

const initialRegisters: Record<string, number> = Array.from({ length: 32 }).reduce<Record<string, number>>((acc, _, i) => {
  acc[`x${i}`] = 0;
  return acc;
}, {});

export const useExecutionStore = create<ExecutionState>((set, get) => ({
  isRunning: false,
  isPaused: false,
  isGdbAttached: false,
  
  pc: 0x10000,
  registers: { ...initialRegisters },
  previousRegisters: { ...initialRegisters },
  csr: {},
  
  instructions: mockInstructions,
  breakpoints: [0x1000c],
  activeTab: 'disassembly',
  
  pipeline: {
    IF: { inst: mockInstructions[0], status: 'valid' },
    ID: { inst: null, status: 'empty' },
    EX: { inst: null, status: 'empty' },
    MEM: { inst: null, status: 'empty' },
    WB: { inst: null, status: 'empty' },
  },
  
  stats: {
    cycles: 0,
    instCount: 0,
    cpi: 0,
    stalls: 0,
    flushes: 0,
  },
  
  logs: [
    { id: '1', timestamp: Date.now(), type: 'info', message: 'Simulator initialized.' },
    { id: '2', timestamp: Date.now(), type: 'info', message: 'Loaded ELF file: riscv_program.elf' },
  ],

  toggleRun: () => set((state) => ({ 
    isRunning: !state.isRunning, 
    isPaused: state.isRunning 
  })),

  step: () => {
    // 模拟一次步进执行的逻辑
    set((state) => {
      const currentInstIndex = state.instructions.findIndex(inst => inst.address === state.pc);
      if (currentInstIndex === -1 || currentInstIndex >= state.instructions.length - 1) return state;
      
      const nextInst = state.instructions[currentInstIndex + 1];
      
      // 模拟流水线推进
      const newPipeline = {
        IF: { inst: state.instructions[currentInstIndex + 2] || null, status: 'valid' as const },
        ID: { inst: nextInst, status: 'valid' as const },
        EX: { inst: state.instructions[currentInstIndex], status: 'valid' as const },
        MEM: state.pipeline.EX,
        WB: state.pipeline.MEM,
      };

      // 模拟寄存器变化
      const prevRegs = { ...state.registers };
      const newRegs = { ...state.registers };
      
      // 简单模拟一下 x1, x2, x3 的变化
      if (state.pc === 0x10000) newRegs['x1'] = 0;
      if (state.pc === 0x10004) newRegs['x2'] = 1;
      if (state.pc === 0x10008) newRegs['x3'] = (newRegs['x1'] || 0) + (newRegs['x2'] || 0);

      // 如果碰到 ecall 模拟 syscall 输出
      let newLogs = [...state.logs];
      if (state.pc === 0x1000c) {
        newLogs.push({
          id: Date.now().toString(),
          timestamp: Date.now(),
          type: 'syscall',
          message: 'ecall triggered: sys_write (fd=1, "Hello RISC-V")'
        });
      }

      return {
        pc: nextInst.address,
        previousRegisters: prevRegs,
        registers: newRegs,
        pipeline: newPipeline,
        stats: {
          ...state.stats,
          cycles: state.stats.cycles + 1,
          instCount: state.stats.instCount + 1,
          cpi: (state.stats.cycles + 1) / (state.stats.instCount + 1),
        },
        logs: newLogs
      };
    });
  },

  reset: () => set(() => ({
    pc: 0x10000,
    registers: { ...initialRegisters },
    previousRegisters: { ...initialRegisters },
    pipeline: {
      IF: { inst: mockInstructions[0], status: 'valid' },
      ID: { inst: null, status: 'empty' },
      EX: { inst: null, status: 'empty' },
      MEM: { inst: null, status: 'empty' },
      WB: { inst: null, status: 'empty' },
    },
    stats: { cycles: 0, instCount: 0, cpi: 0, stalls: 0, flushes: 0 },
    logs: [{ id: Date.now().toString(), timestamp: Date.now(), type: 'info', message: 'CPU Reset.' }]
  })),

  toggleBreakpoint: (address) => set((state) => {
    const exists = state.breakpoints.includes(address);
    return {
      breakpoints: exists 
        ? state.breakpoints.filter(bp => bp !== address)
        : [...state.breakpoints, address]
    };
  }),

  setActiveTab: (tab) => set({ activeTab: tab }),

  addLog: (type, message) => set((state) => ({
    logs: [...state.logs, { id: Date.now().toString(), timestamp: Date.now(), type, message }]
  })),
}));
