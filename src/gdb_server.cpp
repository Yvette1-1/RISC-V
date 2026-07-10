#include "gdb_server.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
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

bool send_char(int sock, char ch) {
#ifdef _WIN32
    return send(sock, &ch, 1, 0) == 1;
#else
    return ::send(sock, &ch, 1, 0) == 1;
#endif
}

std::uint8_t checksum_of(const std::string& payload) {
    std::uint8_t checksum = 0;
    for (unsigned char ch : payload) {
        checksum = static_cast<std::uint8_t>(checksum + ch);
    }
    return checksum;
}

char hex_digit(unsigned value) {
    constexpr char kHex[] = "0123456789abcdef";
    return kHex[value & 0x0f];
}

bool send_ack(int sock) {
    return send_char(sock, '+');
}

bool send_nack(int sock) {
    return send_char(sock, '-');
}

bool send_packet(int sock, const std::string& payload) {
    std::string packet;
    packet.reserve(payload.size() + 4);
    packet.push_back('$');
    packet += payload;
    packet.push_back('#');
    const auto csum = checksum_of(payload);
    packet.push_back(hex_digit((csum >> 4) & 0x0f));
    packet.push_back(hex_digit(csum & 0x0f));

#ifdef _WIN32
    const auto sent = send(sock, packet.data(), static_cast<int>(packet.size()), 0);
#else
    const auto sent = ::send(sock, packet.data(), packet.size(), 0);
#endif
    return sent == static_cast<int>(packet.size());
}

bool send_packet_payload_only(int sock, const std::string& payload) {
    return send_packet(sock, payload);
}

bool recv_hex_digit(char ch, std::uint8_t& value) {
    if (ch >= '0' && ch <= '9') {
        value = static_cast<std::uint8_t>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        value = static_cast<std::uint8_t>(ch - 'a' + 10);
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        value = static_cast<std::uint8_t>(ch - 'A' + 10);
        return true;
    }
    return false;
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

    int reuse = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

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
        if (ch == 0x03) {
            return Packet{PacketKind::Data, "\x03"};
        }
        if (ch != '$') {
            continue;
        }

        std::string payload;
        payload.reserve(64);
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
                std::uint8_t hi = 0;
                std::uint8_t lo = 0;
                if (!recv_hex_digit(c1, hi) || !recv_hex_digit(c2, lo)) {
                    if (!send_nack(sock)) {
                        return std::nullopt;
                    }
                    break;
                }
                const auto expected = static_cast<std::uint8_t>((hi << 4) | lo);
                const auto actual = checksum_of(payload);
                if (actual != expected) {
                    if (!send_nack(sock)) {
                        return std::nullopt;
                    }
                    break;
                }
                if (!send_ack(sock)) {
                    return std::nullopt;
                }
                return Packet{payload.empty() ? PacketKind::Empty : PacketKind::Data, std::move(payload)};
            }
            if (ch == '}') {
                if (!recv_char(sock, ch)) {
                    return std::nullopt;
                }
                payload.push_back(static_cast<char>(ch ^ 0x20));
                continue;
            }
            payload.push_back(ch);
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
            if (packet->payload == "\x03") {
                const auto reply = GdbRspSession::encode_packet("S02");
                if (!send_packet_payload_only(static_cast<int>(client_sock), reply.substr(1, reply.size() - 4))) {
                    break;
                }
                continue;
            }

            const auto reply_payload = session_.handle_packet(packet->payload);
            if (!send_packet(static_cast<int>(client_sock), reply_payload)) {
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

}  // namespace riscv
