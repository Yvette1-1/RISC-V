import React, { useEffect, useRef } from 'react';
import { useExecutionStore } from '@/store/useExecutionStore';
import { clsx } from 'clsx';
import { motion } from 'framer-motion';

const DisassemblyView = () => {
  const { instructions, pc, breakpoints, toggleBreakpoint } = useExecutionStore();
  const activeRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (activeRef.current) {
      activeRef.current.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
  }, [pc]);

  return (
    <div className="flex-1 overflow-y-auto font-mono text-sm">
      {instructions.map((inst) => {
        const isActive = inst.address === pc;
        const isBp = breakpoints.includes(inst.address);
        
        return (
          <div 
            key={inst.address}
            ref={isActive ? activeRef : null}
            className={clsx(
              "flex items-center px-4 py-1.5 cursor-pointer border-l-2",
              isActive ? "bg-blue-500/10 border-blue-500" : "border-transparent hover:bg-zinc-800/50"
            )}
            onClick={() => toggleBreakpoint(inst.address)}
          >
            <div className="w-6 flex justify-center">
              {isBp && <div className="w-2.5 h-2.5 rounded-full bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]" />}
            </div>
            <div className={clsx("w-24", isActive ? "text-blue-400 font-bold" : "text-zinc-500")}>
              0x{inst.address.toString(16).padStart(8, '0')}
            </div>
            <div className="w-24 text-zinc-600">{inst.hex}</div>
            <div className={clsx("flex-1", isActive ? "text-blue-100" : "text-zinc-300")}>
              {inst.asm}
            </div>
          </div>
        );
      })}
    </div>
  );
};

const PipelineView = () => {
  const { pipeline } = useExecutionStore();
  const stages = ['IF', 'ID', 'EX', 'MEM', 'WB'] as const;

  return (
    <div className="p-8 flex flex-col items-center justify-center h-full">
      <div className="flex gap-4">
        {stages.map((stage) => {
          const s = pipeline[stage];
          return (
            <div key={stage} className="flex flex-col items-center gap-4">
              <div className="text-sm font-bold text-zinc-500">{stage}</div>
              <motion.div 
                layout
                className={clsx(
                  "w-32 h-32 rounded-xl border-2 flex flex-col items-center justify-center p-4 relative overflow-hidden",
                  s.status === 'valid' ? "border-blue-500/50 bg-blue-500/10" : 
                  s.status === 'stall' ? "border-yellow-500/50 bg-yellow-500/10" :
                  s.status === 'flush' ? "border-purple-500/50 bg-purple-500/10" :
                  "border-zinc-800 bg-zinc-900/50"
                )}
              >
                {s.status === 'stall' && <div className="absolute inset-0 bg-yellow-500/10 animate-pulse" />}
                {s.status === 'flush' && <div className="absolute inset-0 bg-purple-500/20" />}
                
                {s.inst ? (
                  <>
                    <div className="text-xs font-mono text-zinc-400 mb-2">
                      0x{s.inst.address.toString(16)}
                    </div>
                    <div className="text-sm font-mono text-zinc-100 text-center font-semibold">
                      {s.inst.asm}
                    </div>
                  </>
                ) : (
                  <div className="text-zinc-600 text-sm">Bubble</div>
                )}
              </motion.div>
            </div>
          );
        })}
      </div>
    </div>
  );
};

const MemoryHeatmap = () => {
  return (
    <div className="p-8 flex flex-col h-full text-zinc-400">
      <h3 className="text-lg font-medium text-zinc-200 mb-6">Memory Access Heatmap</h3>
      <div className="flex-1 flex gap-4">
        {/* Simple mock representation */}
        <div className="flex-1 border border-zinc-800 rounded bg-zinc-900/50 p-4 flex flex-col">
          <div className="text-xs mb-2">0x00000000 - 0x04000000 (64MB)</div>
          <div className="flex-1 w-full bg-zinc-950 flex flex-col gap-1 p-2 rounded">
            <div className="h-12 bg-blue-500/30 border border-blue-500/50 rounded flex items-center px-4"><span className="text-xs text-blue-200">.text (R-X) - Hot</span></div>
            <div className="h-8 bg-emerald-500/20 border border-emerald-500/30 rounded flex items-center px-4"><span className="text-xs text-emerald-200">.rodata (R--)</span></div>
            <div className="h-16 bg-orange-500/20 border border-orange-500/30 rounded flex items-center px-4"><span className="text-xs text-orange-200">.data (RW-)</span></div>
            <div className="h-16 bg-yellow-500/20 border border-yellow-500/30 rounded flex items-center px-4"><span className="text-xs text-yellow-200">.bss (RW-)</span></div>
            <div className="flex-1" />
            <div className="h-24 bg-purple-500/20 border border-purple-500/30 rounded flex items-end px-4 py-2"><span className="text-xs text-purple-200">Heap &uarr;</span></div>
            <div className="flex-1" />
            <div className="h-12 bg-red-500/20 border border-red-500/30 rounded flex items-start px-4 py-2"><span className="text-xs text-red-200">Stack &darr; (Hot)</span></div>
          </div>
        </div>
      </div>
    </div>
  );
};

export const Workspace = () => {
  const { activeTab, setActiveTab } = useExecutionStore();

  const tabs = [
    { id: 'disassembly', label: 'Disassembly' },
    { id: 'source', label: 'Source Mapping' },
    { id: 'pipeline', label: 'Pipeline' },
    { id: 'memory', label: 'Memory Heatmap' },
  ] as const;

  return (
    <div className="flex-1 flex flex-col bg-[#0a0a0a] min-w-0">
      <div className="flex bg-zinc-950 border-b border-zinc-800 px-2 pt-2">
        {tabs.map(tab => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id as any)}
            className={clsx(
              "px-4 py-2 text-sm font-medium rounded-t-md transition-colors",
              activeTab === tab.id 
                ? "bg-[#0a0a0a] text-blue-400 border-t border-x border-zinc-800" 
                : "text-zinc-500 hover:text-zinc-300 hover:bg-zinc-900/50"
            )}
          >
            {tab.label}
          </button>
        ))}
      </div>
      
      <div className="flex-1 overflow-hidden flex flex-col">
        {activeTab === 'disassembly' && <DisassemblyView />}
        {activeTab === 'pipeline' && <PipelineView />}
        {activeTab === 'memory' && <MemoryHeatmap />}
        {activeTab === 'source' && (
          <div className="flex-1 flex items-center justify-center text-zinc-600">
            Source mapping not available for this ELF.
          </div>
        )}
      </div>
    </div>
  );
};
