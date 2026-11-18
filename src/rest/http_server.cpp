#include "rest/http_server.hpp"
#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace corecoin::__rest {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto request_deadline = std::chrono::seconds(15);

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
    SocketSystem(const SocketSystem&) = delete;
    SocketSystem& operator=(const SocketSystem&) = delete;
};

class Socket {
public:
    explicit Socket(NativeSocket value) : value_(value) {
        if (value_ == invalid_socket) throw std::runtime_error("Socket operation failed");
    }
    ~Socket() { close_socket(value_); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    [[nodiscard]] NativeSocket get() const noexcept { return value_; }
private:
    NativeSocket value_;
};

class ConnectionClosed : public std::runtime_error {
public:
    ConnectionClosed() : std::runtime_error("Connection closed") {}
};

bool wait_readable(NativeSocket socket, long milliseconds) {
    fd_set ready;
    FD_ZERO(&ready);
    FD_SET(socket, &ready);
    timeval timeout{milliseconds / 1000, static_cast<decltype(timeval::tv_usec)>((milliseconds % 1000) * 1000)};
    return select(static_cast<int>(socket + 1), &ready, nullptr, nullptr, &timeout) > 0;
}

void receive_some(NativeSocket socket, std::string& buffer, Clock::time_point deadline) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (remaining <= 0 || !wait_readable(socket, static_cast<long>(remaining)))
        throw HttpError(408, "request_timeout", "Request was not received in time");
    std::array<char, 4096> chunk{};
#ifdef _WIN32
    const int received = recv(socket, chunk.data(), static_cast<int>(chunk.size()), 0);
#else
    const auto received = recv(socket, chunk.data(), chunk.size(), 0);
#endif
    if (received <= 0) throw ConnectionClosed();
    buffer.append(chunk.data(), static_cast<std::size_t>(received));
}

void send_all(NativeSocket socket, std::string_view data) {
    while (!data.empty()) {
#ifdef _WIN32
        const int sent = send(socket, data.data(), static_cast<int>(data.size()), 0);
#else
        const auto sent = send(socket, data.data(), data.size(), MSG_NOSIGNAL);
#endif
        if (sent <= 0) return;  // Client went away; nothing else to do.
        data.remove_prefix(static_cast<std::size_t>(sent));
    }
}

void set_send_timeout(NativeSocket socket) {
#ifdef _WIN32
    DWORD timeout = 10000;
#else
    timeval timeout{10, 0};
#endif
    (void)setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
}

HttpRequest read_request(NativeSocket socket) {
    const auto deadline = Clock::now() + request_deadline;
    std::string buffer;
    std::size_t head_end;
    while ((head_end = buffer.find("\r\n\r\n")) == std::string::npos) {
        if (buffer.size() > max_request_head_bytes)
            throw HttpError(431, "headers_too_large", "Request head exceeds " + std::to_string(max_request_head_bytes) + " bytes");
        receive_some(socket, buffer, deadline);
    }
    if (head_end > max_request_head_bytes)
        throw HttpError(431, "headers_too_large", "Request head exceeds " + std::to_string(max_request_head_bytes) + " bytes");
    auto request = parse_request_head(std::string_view(buffer).substr(0, head_end));
    const auto length = content_length(request);
    request.body = buffer.substr(head_end + 4);
    while (request.body.size() < length) receive_some(socket, request.body, deadline);
    request.body.resize(length);
    return request;
}
}

HttpServer::HttpServer(std::string bind_address, std::uint16_t port, Handler handler, ErrorHandler on_error)
    : bind_address_(std::move(bind_address)), port_(port), handler_(std::move(handler)), on_error_(std::move(on_error)) {
    if (bind_address_.empty() || port_ == 0) throw std::invalid_argument("API bind address and port are required");
}

void HttpServer::run(const std::atomic<bool>& stop, const std::function<void()>& on_listening) const {
    SocketSystem system;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    addrinfo* resolved = nullptr;
    if (getaddrinfo(bind_address_.c_str(), std::to_string(port_).c_str(), &hints, &resolved) != 0)
        throw std::runtime_error("Cannot resolve API bind address");
    const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(resolved, freeaddrinfo);

    Socket listener(socket(addresses->ai_family, addresses->ai_socktype, addresses->ai_protocol));
#ifndef _WIN32
    const int reuse = 1;
    (void)setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    if (bind(listener.get(), addresses->ai_addr, static_cast<int>(addresses->ai_addrlen)) != 0 ||
        listen(listener.get(), 16) != 0)
        throw std::runtime_error("Cannot listen on API address " + bind_address_ + ":" + std::to_string(port_));
    if (on_listening) on_listening();

    while (!stop.load()) {
        if (!wait_readable(listener.get(), 250)) continue;
        const auto accepted = accept(listener.get(), nullptr, nullptr);
        if (accepted == invalid_socket) continue;
        Socket client(accepted);
        set_send_timeout(client.get());
        HttpRequest request;
        HttpResponse response;
        try {
            request = read_request(client.get());
            response = handler_(request);
        } catch (const ConnectionClosed&) {
            continue;
        } catch (const HttpError& error) {
            response = on_error_(error);
        } catch (const std::exception& error) {
            response = on_error_(HttpError(500, "internal_error", error.what()));
        }
        send_all(client.get(), serialize_response(response));
        if (access_log_) access_log_(request, response);
    }
}
}
