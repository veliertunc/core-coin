#pragma once
#include "rest/http.hpp"
#include <functional>
#include <string>
#include <vector>

namespace corecoin::__rest {
// Maps METHOD + path patterns such as "/api/v1/blocks/{height}" to handlers.
// Routes are matched in registration order, so register literal paths first.
class Router {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    void add(std::string method, std::string_view pattern, Handler handler);
    // Throws HttpError 404 (no path match) or 405 (path matches another method).
    [[nodiscard]] HttpResponse dispatch(HttpRequest request) const;
private:
    struct Route {
        std::string method;
        std::vector<std::string> segments;
        Handler handler;
    };
    std::vector<Route> routes_;
};
}
