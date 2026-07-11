import React, { useEffect, useState } from 'react';
import { useExecutionStore } from '@/store/useExecutionStore';
import { clsx } from 'clsx';
import { motion, AnimatePresence } from 'framer-motion';

export const CpuStatePanel = () => {
  const { registers, previousRegisters, pc, stats } = useExecutionStore();

  const RegRow = ({ name, value, prevValue }: any) => {
    const isChanged = value !== prevValue;
    const [highlight, setHighlight] = useState(false);

    useEffect(() => {
      if (isChanged) {
        setHighlight(true);
        const timer = setTimeout(() => setHighlight(false), 800);
        return () => clearTimeout(timer);
      }
    }, [value, isChanged]);

    return (
      <div className="flex items-center justify-between py-1 px-2 text-sm font-mono relative">
        <span className="text-zinc-500 w-8">{name}</span>
        <motion.span 
          animate={{ 
            color: highlight ? '#60a5fa' : '#d4d4d8',
            textShadow: highlight ? '0 0 8px rgba(96,165,250,0.8)' : 'none'
          }}
          className="z-10"
        >
          0x{value.toString(16).padStart(8, '0')}
        </motion.span>
        {highlight && (
          <motion.div
            layoutId="highlight-bg"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="absolute inset-0 bg-blue-500/20 rounded pointer-events-none"
          />
        )}
      </div>
    );
  };

  const regKeys = Array.from({ length: 32 }).map((_, i) => `x${i}`);

  return (
    <div className="w-72 border-l border-zinc-800 bg-zinc-950 flex flex-col h-full">
      <div className="p-4 border-b border-zinc-800">
        <div className="text-xs font-semibold text-zinc-500 uppercase tracking-wider mb-2">CPU State</div>
        <div className="flex justify-between items-center bg-zinc-900 rounded p-2 border border-zinc-800">
          <span className="text-zinc-400 font-mono text-sm">PC</span>
          <span className="text-blue-400 font-mono font-bold">
            0x{pc.toString(16).padStart(8, '0')}
          </span>
        </div>
      </div>

      <div className="p-4 border-b border-zinc-800 flex-1 overflow-y-auto">
        <div className="text-xs font-semibold text-zinc-500 uppercase tracking-wider mb-2">Registers</div>
        <div className="grid grid-cols-1 gap-x-2">
          {regKeys.map(k => (
            <RegRow key={k} name={k} value={registers[k]} prevValue={previousRegisters[k]} />
          ))}
        </div>
      </div>

      <div className="p-4 bg-zinc-900/50">
        <div className="text-xs font-semibold text-zinc-500 uppercase tracking-wider mb-2">Performance</div>
        <div className="space-y-2 text-sm font-mono">
          <div className="flex justify-between"><span className="text-zinc-500">Cycles:</span><span className="text-zinc-300">{stats.cycles}</span></div>
          <div className="flex justify-between"><span className="text-zinc-500">Insts:</span><span className="text-zinc-300">{stats.instCount}</span></div>
          <div className="flex justify-between"><span className="text-zinc-500">CPI:</span><span className="text-emerald-400">{stats.cpi.toFixed(2)}</span></div>
          <div className="flex justify-between"><span className="text-zinc-500">Stalls:</span><span className="text-yellow-500">{stats.stalls}</span></div>
          <div className="flex justify-between"><span className="text-zinc-500">Flushes:</span><span className="text-purple-400">{stats.flushes}</span></div>
        </div>
      </div>
    </div>
  );
};
