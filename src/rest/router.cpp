#include "rest/router.hpp"
#include <utility>

namespace corecoin::__rest {
namespace {
std::vector<std::string> split_path(std::string_view path) {
    std::vector<std::string> segments;
    while (!path.empty()) {
        if (path.front() == '/') { path.remove_prefix(1); continue; }
        const auto slash = path.find('/');
        segments.emplace_back(path.substr(0, slash));
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash);
    }
    return segments;
}
bool is_parameter(const std::string& segment) {
    return segment.size() > 2 && segment.front() == '{' && segment.back() == '}';
}
}

void Router::add(std::string method, std::string_view pattern, Handler handler) {
    routes_.push_back({std::move(method), split_path(pattern), std::move(handler)});
}

HttpResponse Router::dispatch(HttpRequest request) const {
    const auto segments = split_path(request.path);
    std::string allowed;
    for (const auto& route : routes_) {
        if (route.segments.size() != segments.size()) continue;
        std::map<std::string, std::string> params;
        bool matched = true;
        for (std::size_t i = 0; i < segments.size() && matched; ++i) {
            const auto& expected = route.segments[i];
            if (is_parameter(expected)) params[expected.substr(1, expected.size() - 2)] = percent_decode(segments[i]);
            else matched = expected == segments[i];
        }
        if (!matched) continue;
        if (route.method != request.method) {
            if (allowed.find(route.method) == std::string::npos) allowed += (allowed.empty() ? "" : ", ") + route.method;
            continue;
        }
        request.params = std::move(params);
        return route.handler(request);
    }
    if (!allowed.empty())
        throw HttpError(405, "method_not_allowed", "Allowed methods: " + allowed);
    throw HttpError(404, "not_found", "No endpoint for " + request.path);
}
}
