#include "gdb_server.hpp"

#include <chrono>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

namespace riscv {

GdbServer::GdbServer(Simulator& simulator) : simulator_(simulator), session_(simulator) {}
GdbServer::~GdbServer() { stop(); }

bool GdbServer::start(std::uint16_t port) {
    if (running_) return true;
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
    const socket_t sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == kInvalidSocket) return false;
    listen_sock_ = static_cast<int>(sock);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
    if (listen(sock, 1) != 0) return false;
    running_ = true;
    std::thread([this]() { serve(); }).detach();
    return true;
}

void GdbServer::stop() {
    running_ = false;
    if (listen_sock_ != -1) {
#ifdef _WIN32
        closesocket(static_cast<socket_t>(listen_sock_));
        WSACleanup();
#else
        close(listen_sock_);
#endif
        listen_sock_ = -1;
    }
}

bool GdbServer::running() const noexcept { return running_; }

void GdbServer::serve() {
    while (running_) {
        sockaddr_in client{};
#ifdef _WIN32
        int len = sizeof(client);
#else
        socklen_t len = sizeof(client);
#endif
        const auto client_sock = accept(static_cast<socket_t>(listen_sock_), reinterpret_cast<sockaddr*>(&client), &len);
        if (client_sock == kInvalidSocket) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        while (running_) {
            const auto line = recv_line(static_cast<int>(client_sock));
            if (line.empty()) break;
            std::string payload;
            if (!GdbRspSession::decode_packet(line, payload)) {
                continue;
            }
            const auto reply_payload = session_.handle_packet(payload);
            const auto reply = GdbRspSession::encode_packet(reply_payload.empty() ? "" : reply_payload);
            if (!send_all(static_cast<int>(client_sock), reply)) break;
        }
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
    }
}

std::string GdbServer::recv_line(int sock) {
    std::string out;
    char ch = 0;
    while (true) {
#ifdef _WIN32
        const auto n = recv(sock, &ch, 1, 0);
#else
        const auto n = ::recv(sock, &ch, 1, 0);
#endif
        if (n <= 0) break;
        out.push_back(ch);
        if (ch == '#') {
            char c1 = 0, c2 = 0;
#ifdef _WIN32
            if (recv(sock, &c1, 1, 0) <= 0 || recv(sock, &c2, 1, 0) <= 0) break;
#else
            if (::recv(sock, &c1, 1, 0) <= 0 || ::recv(sock, &c2, 1, 0) <= 0) break;
#endif
            out.push_back(c1);
            out.push_back(c2);
            break;
        }
    }
    return out;
}

bool GdbServer::send_all(int sock, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
#ifdef _WIN32
        const auto n = send(sock, data.data() + sent, static_cast<int>(data.size() - sent), 0);
#else
        const auto n = ::send(sock, data.data() + sent, data.size() - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace riscv
