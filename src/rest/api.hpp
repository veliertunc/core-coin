#pragma once
#include "rest/http.hpp"
#include "rest/json.hpp"
#include "rest/node_service.hpp"
#include "rest/router.hpp"
#include <string>

namespace corecoin::__rest {
inline constexpr std::string_view api_prefix = "/api/v1";

struct ApiOptions {
    // Empty disables CORS; "*" allows any origin; otherwise one exact origin.
    std::string cors_origin;
};

[[nodiscard]] HttpResponse json_response(int status, const Json& body);
[[nodiscard]] HttpResponse error_response(int status, const std::string& code, const std::string& message);

// Version 1 REST endpoints. Converts every failure into a JSON error response.
class Api {
public:
    Api(NodeService& node, ApiOptions options = {});
    [[nodiscard]] HttpResponse handle(const HttpRequest& request) const;
private:
    void register_routes();
    void apply_cors(const HttpRequest& request, HttpResponse& response) const;
    NodeService& node_;
    ApiOptions options_;
    Router router_;
};
}
