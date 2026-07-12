import React from 'react';
import { Toolbar } from './Toolbar';
import { Sidebar } from './Sidebar';
import { Workspace } from './Workspace';
import { CpuStatePanel } from './CpuStatePanel';
import { ConsolePanel } from './ConsolePanel';

export const ExecutionStudio = () => {
  return (
    <div className="flex flex-col h-screen w-full bg-zinc-950 text-zinc-300 font-sans overflow-hidden selection:bg-blue-500/30">
      <Toolbar />
      <div className="flex flex-1 overflow-hidden">
        <Sidebar />
        <div className="flex flex-col flex-1 min-w-0">
          <Workspace />
          <ConsolePanel />
        </div>
        <CpuStatePanel />
      </div>
    </div>
  );
};
