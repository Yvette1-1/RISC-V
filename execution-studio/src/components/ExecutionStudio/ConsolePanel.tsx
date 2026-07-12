import React from 'react';
import { Terminal, ShieldAlert, Activity, Bug } from 'lucide-react';
import { useExecutionStore } from '@/store/useExecutionStore';
import { clsx } from 'clsx';

export const ConsolePanel = () => {
  const logs = useExecutionStore(state => state.logs);

  const getLogIcon = (type: string) => {
    switch (type) {
      case 'error': return <ShieldAlert size={14} className="text-red-400 mt-0.5" />;
      case 'syscall': return <Activity size={14} className="text-purple-400 mt-0.5" />;
      case 'gdb': return <Bug size={14} className="text-blue-400 mt-0.5" />;
      default: return <Terminal size={14} className="text-zinc-500 mt-0.5" />;
    }
  };

  return (
    <div className="h-48 border-t border-zinc-800 bg-zinc-950 flex flex-col">
      <div className="flex items-center gap-4 px-4 h-8 border-b border-zinc-800 bg-zinc-900/50 text-xs font-medium text-zinc-400">
        <button className="text-zinc-200 border-b-2 border-blue-500 h-full">Console Output</button>
        <button className="hover:text-zinc-200 h-full">GDB RSP Log</button>
        <button className="hover:text-zinc-200 h-full">Syscall Trace</button>
      </div>
      
      <div className="flex-1 overflow-y-auto p-2 font-mono text-sm">
        {logs.map((log) => (
          <div key={log.id} className="flex gap-3 py-1 hover:bg-zinc-800/30 px-2 rounded">
            {getLogIcon(log.type)}
            <span className="text-zinc-600">[{new Date(log.timestamp).toLocaleTimeString()}]</span>
            <span className={clsx(
              log.type === 'error' && "text-red-400",
              log.type === 'syscall' && "text-purple-300",
              log.type === 'gdb' && "text-blue-300",
              log.type === 'info' && "text-zinc-300"
            )}>
              {log.message}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
};
