import React from 'react';
import { Play, Pause, StepForward, RotateCcw, Square, MonitorDot, Download, FileCode } from 'lucide-react';
import { useExecutionStore } from '@/store/useExecutionStore';
import { clsx } from 'clsx';

export const Toolbar = () => {
  const { isRunning, isPaused, toggleRun, step, reset } = useExecutionStore();

  const Button = ({ icon: Icon, label, onClick, active, variant = 'default' }: any) => (
    <button
      onClick={onClick}
      className={clsx(
        "flex items-center gap-2 px-3 py-1.5 rounded text-sm font-medium transition-colors",
        active ? "bg-zinc-800 text-white" : "text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800",
        variant === 'primary' && "bg-blue-600/20 text-blue-400 hover:bg-blue-600/30",
        variant === 'danger' && "hover:bg-red-900/30 hover:text-red-400"
      )}
    >
      <Icon size={16} />
      <span>{label}</span>
    </button>
  );

  return (
    <div className="h-12 border-b border-zinc-800 bg-zinc-950 flex items-center justify-between px-4">
      <div className="flex items-center gap-2">
        <div className="text-zinc-100 font-semibold mr-6 flex items-center gap-2">
          <MonitorDot className="text-blue-500" size={18} />
          RISC-V Studio
        </div>
        
        <div className="h-6 w-px bg-zinc-800 mx-2" />
        
        <Button icon={FileCode} label="Open ELF" />
        
        <div className="h-6 w-px bg-zinc-800 mx-2" />

        <Button 
          icon={isRunning && !isPaused ? Pause : Play} 
          label={isRunning && !isPaused ? "Pause" : "Run"} 
          onClick={toggleRun}
          variant={isRunning && !isPaused ? "default" : "primary"}
        />
        <Button icon={StepForward} label="Step" onClick={step} />
        <Button icon={StepForward} label="Step Over" />
        <Button icon={RotateCcw} label="Reset" onClick={reset} />
        <Button icon={Square} label="Stop" variant="danger" />
      </div>

      <div className="flex items-center gap-2">
        <Button icon={MonitorDot} label="Attach GDB" />
        <Button icon={Download} label="Export Trace" />
      </div>
    </div>
  );
};
