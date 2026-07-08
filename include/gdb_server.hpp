#pragma once

#include "gdb_rsp.hpp"
#include "simulator.hpp"

#include <cstdint>
#include <string>

namespace riscv {

class GdbServer {
public:
    explicit GdbServer(Simulator& simulator);
    ~GdbServer();

    bool start(std::uint16_t port = 1234);
    void stop();
    bool running() const noexcept;

private:
    void serve();
    static std::string recv_line(int sock);
    static bool send_all(int sock, const std::string& data);

    Simulator& simulator_;
    GdbRspSession session_;
    bool running_ = false;
    int listen_sock_ = -1;
};

}  // namespace riscv
