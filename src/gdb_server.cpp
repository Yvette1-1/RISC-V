#include "gdb_server.hpp"

#include <chrono>
#include <cstring>
#include <optional>
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
namespace {

bool recv_char(int sock, char& ch) {
#ifdef _WIN32
    const auto n = recv(sock, &ch, 1, 0);
#else
    const auto n = ::recv(sock, &ch, 1, 0);
#endif
    return n > 0;
}

}  // namespace

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
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return false;
    }
    if (listen(sock, 1) != 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return false;
    }
    listen_sock_ = static_cast<int>(sock);
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

std::optional<GdbServer::Packet> GdbServer::recv_packet(int sock) {
    char ch = 0;
    while (true) {
        if (!recv_char(sock, ch)) {
            return std::nullopt;
        }
        if (ch == '+') {
            return Packet{PacketKind::Ack, {}};
        }
        if (ch == '-') {
            return Packet{PacketKind::Nack, {}};
        }
        if (ch == '$') {
            std::string payload;
            while (true) {
                if (!recv_char(sock, ch)) {
                    return std::nullopt;
                }
                if (ch == '#') {
                    char c1 = 0;
                    char c2 = 0;
                    if (!recv_char(sock, c1) || !recv_char(sock, c2)) {
                        return std::nullopt;
                    }
                    if (payload.empty()) {
                        return Packet{PacketKind::Empty, {}};
                    }
                    return Packet{PacketKind::Data, std::move(payload)};
                }
                payload.push_back(ch);
            }
        }
    }
}

void GdbServer::serve() {
    while (running_) {
        sockaddr_in client{};
#ifdef _WIN32
        int len = sizeof(client);
#else
        socklen_t len = sizeof(client);
#endif
        const auto listen_socket = static_cast<socket_t>(listen_sock_);
        const auto client_sock = accept(listen_socket, reinterpret_cast<sockaddr*>(&client), &len);
        if (client_sock == kInvalidSocket) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        while (running_) {
            const auto packet = recv_packet(static_cast<int>(client_sock));
            if (!packet.has_value()) {
                break;
            }
            if (packet->kind == PacketKind::Ack || packet->kind == PacketKind::Nack || packet->kind == PacketKind::Empty) {
                continue;
            }

            const auto reply_payload = session_.handle_packet(packet->payload);
            const auto reply = GdbRspSession::encode_packet(reply_payload);
            if (!send_all(static_cast<int>(client_sock), reply)) {
                break;
            }
        }
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
    }
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
