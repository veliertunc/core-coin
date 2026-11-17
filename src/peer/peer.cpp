#include "corecoin/peer.hpp"
#include "corecoin/chain_store.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#endif

namespace corecoin {
namespace {
constexpr std::uint32_t max_chain_bytes = 4 * 1024 * 1024;
constexpr std::string_view request = "CC01";
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket invalid_socket = INVALID_SOCKET;
void close_socket(NativeSocket value) { closesocket(value); }
#else
using NativeSocket = int;
constexpr NativeSocket invalid_socket = -1;
void close_socket(NativeSocket value) { close(value); }
#endif
class SocketSystem {
public:
    SocketSystem() {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("Cannot initialize sockets");
#endif
    }
    ~SocketSystem() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
class Socket {
public:
    explicit Socket(NativeSocket value) : value_(value) { if (value_ == invalid_socket) throw std::runtime_error("Socket operation failed"); }
    ~Socket() { close_socket(value_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    [[nodiscard]] NativeSocket get() const { return value_; }
private:
    NativeSocket value_;
};
void timeouts(NativeSocket socket) {
#ifdef _WIN32
    DWORD timeout = 10000;
    const char* value = reinterpret_cast<const char*>(&timeout);
#else
    timeval timeout{10, 0};
    const void* value = &timeout;
#endif
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, static_cast<const char*>(value), sizeof(timeout)) != 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, static_cast<const char*>(value), sizeof(timeout)) != 0)
        throw std::runtime_error("Cannot set socket timeout");
}
using AddressList = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>;
AddressList resolve(const std::string& host, std::uint16_t port, bool listening) {
    if (host.empty() || port == 0) throw std::invalid_argument("Host and port are required");
    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (listening) hints.ai_flags = AI_PASSIVE;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0)
        throw std::runtime_error("Cannot resolve peer address");
    return AddressList(result, freeaddrinfo);
}
void send_all(NativeSocket socket, const char* data, std::size_t size) {
    while (size) {
#ifdef _WIN32
        const int sent = send(socket, data, static_cast<int>(size), 0);
#else
        const auto sent = send(socket, data, size, MSG_NOSIGNAL);
#endif
        if (sent <= 0) throw std::runtime_error("Peer send failed");
        data += sent; size -= static_cast<std::size_t>(sent);
    }
}
void receive_all(NativeSocket socket, char* data, std::size_t size) {
    while (size) {
#ifdef _WIN32
        const int received = recv(socket, data, static_cast<int>(size), 0);
#else
        const auto received = recv(socket, data, size, 0);
#endif
        if (received <= 0) throw std::runtime_error("Peer response incomplete or timed out");
        data += received; size -= static_cast<std::size_t>(received);
    }
}
}
void Peer::serve_once(const std::string& bind_address, std::uint16_t port, const Blockchain& chain) {
    std::ostringstream encoded;
    ChainCodec::write(encoded, chain);
    const auto payload = encoded.str();
    if (payload.size() > max_chain_bytes) throw std::length_error("Chain exceeds peer transfer limit");
    SocketSystem system;
    auto addresses = resolve(bind_address, port, true);
    Socket listener(socket(addresses->ai_family, addresses->ai_socktype, addresses->ai_protocol));
    if (bind(listener.get(), addresses->ai_addr, static_cast<int>(addresses->ai_addrlen)) != 0 || listen(listener.get(), 1) != 0)
        throw std::runtime_error("Cannot listen on peer address");
    fd_set ready; FD_ZERO(&ready); FD_SET(listener.get(), &ready);
    timeval timeout{30, 0};
    if (select(static_cast<int>(listener.get() + 1), &ready, nullptr, nullptr, &timeout) <= 0)
        throw std::runtime_error("Peer accept timed out");
    Socket client(accept(listener.get(), nullptr, nullptr));
    timeouts(client.get());
    std::array<char, 4> command{};
    receive_all(client.get(), command.data(), command.size());
    if (std::string_view(command.data(), command.size()) != request) throw std::runtime_error("Unknown peer protocol");
    const auto length = static_cast<std::uint32_t>(payload.size());
    const std::array<char, 4> header{static_cast<char>(length >> 24), static_cast<char>(length >> 16),
                                     static_cast<char>(length >> 8), static_cast<char>(length)};
    send_all(client.get(), header.data(), header.size());
    send_all(client.get(), payload.data(), payload.size());
}
Blockchain Peer::fetch(const std::string& host, std::uint16_t port) {
    SocketSystem system;
    auto addresses = resolve(host, port, false);
    Socket connection(socket(addresses->ai_family, addresses->ai_socktype, addresses->ai_protocol));
    timeouts(connection.get());
    if (connect(connection.get(), addresses->ai_addr, static_cast<int>(addresses->ai_addrlen)) != 0)
        throw std::runtime_error("Cannot connect to peer");
    send_all(connection.get(), request.data(), request.size());
    std::array<unsigned char, 4> header{};
    receive_all(connection.get(), reinterpret_cast<char*>(header.data()), header.size());
    const auto size = (std::uint32_t(header[0]) << 24) | (std::uint32_t(header[1]) << 16) |
                      (std::uint32_t(header[2]) << 8) | std::uint32_t(header[3]);
    if (size == 0 || size > max_chain_bytes) throw std::length_error("Invalid peer response size");
    std::string payload(size, '\0');
    receive_all(connection.get(), payload.data(), payload.size());
    std::istringstream input(payload);
    return ChainCodec::read(input);
}
}
