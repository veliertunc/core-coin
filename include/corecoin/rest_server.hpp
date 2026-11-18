#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace corecoin {
struct RestConfig {
    std::string bind_address = "127.0.0.1";
    std::uint16_t port = 18445;
    // Empty disables CORS; "*" allows any origin; otherwise one exact origin.
    std::string cors_origin;
    // Optional per-request log line, e.g. "GET /api/v1/node 200".
    std::function<void(std::string_view)> access_log;
};

// JSON REST API (see API.md) over one persistent chain file. Requests are
// served one at a time. There is no authentication: legacy named accounts can
// be spent by any client, so bind only to trusted interfaces.
class RestServer {
public:
    explicit RestServer(std::filesystem::path chain_file, RestConfig config = {});
    ~RestServer();
    RestServer(const RestServer&) = delete;
    RestServer& operator=(const RestServer&) = delete;
    // Blocks until stop() is called. on_listening runs once the socket accepts connections.
    void run(const std::function<void()>& on_listening = {});
    // Safe to call from another thread or a signal handler.
    void stop() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
