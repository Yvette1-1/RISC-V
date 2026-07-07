#include "gdb_rsp.hpp"
#include "gdb_server.hpp"
#include "simulator.hpp"

#include <cassert>
#include <string>

int main() {
    riscv::Simulator sim({});
    riscv::GdbRspSession rsp(sim);

    assert(rsp.handle_packet("?") == "S05");
    assert(rsp.handle_packet("qSupported") == "PacketSize=4000;swbreak+");
    assert(rsp.handle_packet("g").size() == 33u * 8u);
    assert(rsp.handle_packet("Z0,1000,4") == "OK");
    assert(sim.has_breakpoint(0x1000u));
    assert(rsp.handle_packet("z0,1000,4") == "OK");
    assert(!sim.has_breakpoint(0x1000u));

    const auto packet = riscv::GdbRspSession::encode_packet("?");
    std::string payload;
    assert(riscv::GdbRspSession::decode_packet(packet, payload));
    assert(payload == "?");

    (void)sizeof(riscv::GdbServer);
    return 0;
}
