#pragma once
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace corecoin::__rest {
inline constexpr std::size_t max_request_head_bytes = 16 * 1024;
inline constexpr std::size_t max_request_body_bytes = 64 * 1024;

// A protocol or request error that maps directly to an HTTP status and error code.
class HttpError : public std::runtime_error {
public:
    HttpError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), status_(status), code_(std::move(code)) {}
    [[nodiscard]] int status() const noexcept { return status_; }
    [[nodiscard]] const std::string& code() const noexcept { return code_; }
private:
    int status_;
    std::string code_;
};

struct HttpRequest {
    std::string method;
    std::string path;                            // Raw path without the query string.
    std::map<std::string, std::string> query;    // Decoded query parameters.
    std::map<std::string, std::string> headers;  // Lowercase header names.
    std::map<std::string, std::string> params;   // Decoded path parameters, set by Router.
    std::string body;

    [[nodiscard]] const std::string* header(std::string_view name) const;
    [[nodiscard]] std::optional<std::string> query_value(const std::string& name) const;
};

struct HttpResponse {
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    void add_header(std::string name, std::string value) { headers.emplace_back(std::move(name), std::move(value)); }
};

// Parses the request line and headers (everything before the blank line).
[[nodiscard]] HttpRequest parse_request_head(std::string_view head);
// Returns the declared body length, rejecting unsupported or oversized bodies.
[[nodiscard]] std::size_t content_length(const HttpRequest& request);
[[nodiscard]] std::string serialize_response(const HttpResponse& response);
[[nodiscard]] std::string_view status_text(int status) noexcept;
// Decodes %XX escapes; when form is true, '+' also decodes to a space.
[[nodiscard]] std::string percent_decode(std::string_view value, bool form = false);
}
