#include "debugger.hpp"
#include "simulator.hpp"

#include <cassert>

int main() {
    riscv::Simulator sim({});
    riscv::Debugger dbg(sim);

    auto result = dbg.handle_command("help");
    assert(result.handled);

    result = dbg.handle_command("quit");
    assert(result.handled);
    assert(result.should_exit);

    result = dbg.handle_command("regs");
    assert(result.handled);

    result = dbg.handle_command("trace on");
    assert(result.handled);

    result = dbg.handle_command("b 0x1000");
    assert(result.handled);

    result = dbg.handle_command("del 0");
    assert(result.handled);

    result = dbg.handle_command("reset");
    assert(result.handled);

    return 0;
}
