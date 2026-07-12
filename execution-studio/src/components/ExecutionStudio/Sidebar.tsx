import React from 'react';
import { Layers, ListChecks, Bug, Crosshair, Clock } from 'lucide-react';
import { useExecutionStore } from '@/store/useExecutionStore';

export const Sidebar = () => {
  const breakpoints = useExecutionStore(state => state.breakpoints);

  const Section = ({ title, icon: Icon, children }: any) => (
    <div className="mb-6">
      <div className="flex items-center gap-2 text-xs font-semibold text-zinc-500 uppercase tracking-wider mb-3 px-4">
        <Icon size={14} />
        {title}
      </div>
      <div className="px-2">
        {children}
      </div>
    </div>
  );

  const Item = ({ label, value, active }: any) => (
    <div className={`flex justify-between items-center px-2 py-1.5 rounded text-sm cursor-pointer ${active ? 'bg-blue-500/10 text-blue-400' : 'text-zinc-400 hover:text-zinc-200 hover:bg-zinc-800/50'}`}>
      <span>{label}</span>
      {value && <span className="font-mono text-xs opacity-60">{value}</span>}
    </div>
  );

  return (
    <div className="w-64 border-r border-zinc-800 bg-zinc-950/50 overflow-y-auto py-4">
      <Section title="Project / ELF" icon={Layers}>
        <Item label=".text" value="0x10000" active />
        <Item label=".rodata" value="0x20000" />
        <Item label=".data" value="0x30000" />
        <Item label=".bss" value="0x40000" />
      </Section>

      <Section title="Breakpoints" icon={Bug}>
        {breakpoints.length === 0 ? (
          <div className="text-xs text-zinc-600 px-2">No breakpoints</div>
        ) : (
          breakpoints.map(bp => (
            <div key={bp} className="flex items-center gap-2 px-2 py-1.5 text-sm text-zinc-300">
              <div className="w-2 h-2 rounded-full bg-red-500" />
              <span className="font-mono text-xs">0x{bp.toString(16).padStart(8, '0')}</span>
            </div>
          ))
        )}
      </Section>

      <Section title="Watchpoints" icon={Crosshair}>
        <div className="text-xs text-zinc-600 px-2">No watchpoints</div>
      </Section>

      <Section title="Syscall History" icon={Clock}>
        <Item label="sys_write" value="12ms ago" />
        <Item label="sys_brk" value="1s ago" />
      </Section>
    </div>
  );
};
