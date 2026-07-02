#include "debugger.hpp"
#include "simulator.hpp"

#include <cassert>
#include <cstdint>
#include <sstream>

int main() {
    riscv::Simulator sim({});
    riscv::Debugger dbg(sim);

    auto result = dbg.handle_command("");
    assert(result.handled);
    assert(!result.should_exit);

    result = dbg.handle_command("unknown");
    assert(result.handled);
    assert(result.message == "unknown command");

    result = dbg.handle_command("trace");
    assert(result.handled);
    assert(result.message == "trace: missing argument");

    result = dbg.handle_command("trace maybe");
    assert(result.handled);
    assert(result.message == "Instruction trace: OFF");

    result = dbg.handle_command("b nope");
    assert(result.handled);
    assert(result.message == "b: invalid address");

    result = dbg.handle_command("del 0");
    assert(result.handled);
    assert(result.message == "del: out of range");

    result = dbg.handle_command("reset");
    assert(result.handled);
    assert(result.message == "Reset complete");

    result = dbg.handle_command("q");
    assert(result.handled);
    assert(result.should_exit);

    return 0;
}
