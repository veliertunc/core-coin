#include "rest/http.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>

namespace corecoin::__rest {
namespace {
std::string lowercase(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}
bool is_token(std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](unsigned char c) {
        return std::isalnum(c) || std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos;
    });
}
[[noreturn]] void bad_request(const std::string& message) { throw HttpError(400, "bad_request", message); }
}

const std::string* HttpRequest::header(std::string_view name) const {
    const auto it = headers.find(lowercase(name));
    return it == headers.end() ? nullptr : &it->second;
}
std::optional<std::string> HttpRequest::query_value(const std::string& name) const {
    const auto it = query.find(name);
    if (it == query.end()) return std::nullopt;
    return it->second;
}

std::string percent_decode(std::string_view value, bool form) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+' && form) { out += ' '; continue; }
        if (value[i] != '%') { out += value[i]; continue; }
        unsigned byte = 0;
        if (i + 2 >= value.size()) bad_request("Malformed percent-encoding");
        const auto [end, error] = std::from_chars(value.data() + i + 1, value.data() + i + 3, byte, 16);
        if (error != std::errc{} || end != value.data() + i + 3) bad_request("Malformed percent-encoding");
        out += static_cast<char>(byte);
        i += 2;
    }
    return out;
}

HttpRequest parse_request_head(std::string_view head) {
    HttpRequest request;
    const auto line_end = head.find("\r\n");
    const auto request_line = head.substr(0, line_end);
    const auto first_space = request_line.find(' ');
    const auto second_space = request_line.find(' ', first_space == std::string_view::npos ? first_space : first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos ||
        request_line.find(' ', second_space + 1) != std::string_view::npos)
        bad_request("Malformed request line");
    request.method = std::string(request_line.substr(0, first_space));
    const auto target = request_line.substr(first_space + 1, second_space - first_space - 1);
    const auto version = request_line.substr(second_space + 1);
    if (!is_token(request.method)) bad_request("Invalid method");
    if (version != "HTTP/1.1" && version != "HTTP/1.0") throw HttpError(505, "http_version_not_supported", "Only HTTP/1.0 and HTTP/1.1 are supported");
    if (target.empty() || target.front() != '/') bad_request("Request target must be an absolute path");

    const auto question = target.find('?');
    request.path = std::string(target.substr(0, question));
    if (question != std::string_view::npos) {
        auto query = target.substr(question + 1);
        while (!query.empty()) {
            const auto amp = query.find('&');
            const auto pair = query.substr(0, amp);
            if (!pair.empty()) {
                const auto equals = pair.find('=');
                auto name = percent_decode(pair.substr(0, equals), true);
                auto value = equals == std::string_view::npos ? std::string{} : percent_decode(pair.substr(equals + 1), true);
                request.query.insert_or_assign(std::move(name), std::move(value));
            }
            if (amp == std::string_view::npos) break;
            query.remove_prefix(amp + 1);
        }
    }

    auto rest = line_end == std::string_view::npos ? std::string_view{} : head.substr(line_end + 2);
    while (!rest.empty()) {
        const auto end = rest.find("\r\n");
        const auto line = rest.substr(0, end);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || !is_token(line.substr(0, colon))) bad_request("Malformed header");
        auto name = lowercase(line.substr(0, colon));
        const auto value = std::string(trim(line.substr(colon + 1)));
        const auto [it, inserted] = request.headers.try_emplace(name, value);
        if (!inserted) {
            if (name == "content-length" || name == "host") bad_request("Duplicate " + name + " header");
            it->second += ", " + value;
        }
        if (end == std::string_view::npos) break;
        rest.remove_prefix(end + 2);
    }
    return request;
}

std::size_t content_length(const HttpRequest& request) {
    if (request.header("transfer-encoding"))
        throw HttpError(501, "not_implemented", "Transfer-Encoding is not supported; send Content-Length");
    const auto* value = request.header("content-length");
    if (!value) return 0;
    std::size_t length{};
    const auto [end, error] = std::from_chars(value->data(), value->data() + value->size(), length);
    if (value->empty() || error != std::errc{} || end != value->data() + value->size())
        bad_request("Invalid Content-Length");
    if (length > max_request_body_bytes)
        throw HttpError(413, "payload_too_large", "Request body exceeds " + std::to_string(max_request_body_bytes) + " bytes");
    return length;
}

std::string_view status_text(int status) noexcept {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Content";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 505: return "HTTP Version Not Supported";
    default: return "Unknown";
    }
}

std::string serialize_response(const HttpResponse& response) {
    std::string out = "HTTP/1.1 " + std::to_string(response.status) + " " + std::string(status_text(response.status)) + "\r\n";
    for (const auto& [name, value] : response.headers) out += name + ": " + value + "\r\n";
    out += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    out += "Connection: close\r\n\r\n";
    out += response.body;
    return out;
}
}
