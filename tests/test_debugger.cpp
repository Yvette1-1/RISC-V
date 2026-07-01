#include "debugger.hpp"
#include "simulator.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>

int main() {
    riscv::Simulator sim({});
    riscv::Debugger dbg(sim);

    auto result = dbg.handle_command("help");
    assert(result.handled);

    result = dbg.handle_command("trace on");
    assert(result.handled);
    assert(dbg.trace_enabled());

    result = dbg.handle_command("trace off");
    assert(result.handled);
    assert(!dbg.trace_enabled());

    result = dbg.handle_command("b 0x1000");
    assert(result.handled);
    assert(dbg.has_breakpoint(0x1000u));

    result = dbg.handle_command("info b");
    assert(result.handled);

    result = dbg.handle_command("del 0");
    assert(result.handled);
    assert(!dbg.has_breakpoint(0x1000u));

    result = dbg.handle_command("x 0x1000=0x1234");
    assert(result.handled);

    result = dbg.handle_command("reset");
    assert(result.handled);

    result = dbg.handle_command("regs");
    assert(result.handled);

    result = dbg.handle_command("si 1");
    assert(result.handled);

    return 0;
}
