#include "corecoin/rest_server.hpp"
#include "rest/api.hpp"
#include "rest/http_server.hpp"
#include "rest/node_service.hpp"
#include <atomic>
#include <utility>

namespace corecoin {
struct RestServer::Impl {
    Impl(std::filesystem::path chain_file, RestConfig config)
        : node(std::move(chain_file)),
          api(node, __rest::ApiOptions{config.cors_origin}),
          server(config.bind_address, config.port,
                 [this](const __rest::HttpRequest& request) { return api.handle(request); },
                 [](const __rest::HttpError& error) {
                     return __rest::error_response(error.status(), error.code(), error.what());
                 }) {
        if (config.access_log)
            server.set_access_log([log = std::move(config.access_log)](const __rest::HttpRequest& request,
                                                                         const __rest::HttpResponse& response) {
                log((request.method.empty() ? std::string("-") : request.method) + " " +
                    (request.path.empty() ? std::string("-") : request.path) + " " + std::to_string(response.status));
            });
    }
    __rest::NodeService node;
    __rest::Api api;
    __rest::HttpServer server;
    std::atomic<bool> stopping{false};
};

static_assert(std::atomic<bool>::is_always_lock_free, "stop() must be async-signal-safe");

RestServer::RestServer(std::filesystem::path chain_file, RestConfig config)
    : impl_(std::make_unique<Impl>(std::move(chain_file), std::move(config))) {}
RestServer::~RestServer() = default;
void RestServer::run(const std::function<void()>& on_listening) { impl_->server.run(impl_->stopping, on_listening); }
void RestServer::stop() noexcept { impl_->stopping.store(true); }
}
