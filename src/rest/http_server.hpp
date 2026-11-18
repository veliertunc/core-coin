#pragma once
#include "rest/http.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace corecoin::__rest {
// Blocking HTTP/1.1 listener. Connections are handled one at a time and closed
// after one response; the loop checks the stop flag at least every 250 ms.
class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    using ErrorHandler = std::function<HttpResponse(const HttpError&)>;
    using AccessLog = std::function<void(const HttpRequest&, const HttpResponse&)>;

    HttpServer(std::string bind_address, std::uint16_t port, Handler handler, ErrorHandler on_error);
    void set_access_log(AccessLog log) { access_log_ = std::move(log); }
    void run(const std::atomic<bool>& stop, const std::function<void()>& on_listening) const;
private:
    std::string bind_address_;
    std::uint16_t port_;
    Handler handler_;
    ErrorHandler on_error_;
    AccessLog access_log_;
};
}
