#pragma once

#include <cstdint>
#include <string>

namespace riscv {

class Simulator;

class Debugger {
public:
    explicit Debugger(Simulator& simulator);

    void repl();

private:
    Simulator& simulator_;
};

}  // namespace riscv
