#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace corecoin::__rest {
class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Minimal JSON document model for the REST layer. Objects keep insertion order.
class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool value) : value_(value) {}
    template <std::integral T> requires (!std::same_as<T, bool>)
    Json(T value) : value_(to_integer(value)) {}
    Json(double value) : value_(value) {}
    Json(std::string value) : value_(std::move(value)) {}
    Json(std::string_view value) : value_(std::string(value)) {}
    Json(const char* value) : value_(std::string(value)) {}
    Json(Array value) : value_(std::move(value)) {}
    Json(Object value) : value_(std::move(value)) {}

    [[nodiscard]] static Json object() { return Json(Object{}); }
    [[nodiscard]] static Json array() { return Json(Array{}); }
    [[nodiscard]] static Json parse(std::string_view text);

    [[nodiscard]] bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value_); }
    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool is_integer() const noexcept { return std::holds_alternative<std::int64_t>(value_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
    [[nodiscard]] bool is_array() const noexcept { return std::holds_alternative<Array>(value_); }
    [[nodiscard]] bool is_object() const noexcept { return std::holds_alternative<Object>(value_); }

    [[nodiscard]] bool as_bool() const { return get<bool>("boolean"); }
    [[nodiscard]] std::int64_t as_integer() const { return get<std::int64_t>("integer"); }
    [[nodiscard]] const std::string& as_string() const { return get<std::string>("string"); }
    [[nodiscard]] const Array& as_array() const { return get<Array>("array"); }
    [[nodiscard]] const Object& as_object() const { return get<Object>("object"); }

    // Object member lookup; returns nullptr when absent or when this is not an object.
    [[nodiscard]] const Json* find(std::string_view key) const;
    Json& set(std::string key, Json value);
    Json& push(Json value);
    [[nodiscard]] std::string dump() const;

private:
    template <std::integral T>
    static std::int64_t to_integer(T value) {
        if constexpr (std::is_unsigned_v<T> && sizeof(T) >= sizeof(std::int64_t)) {
            if (value > static_cast<T>(std::numeric_limits<std::int64_t>::max())) throw JsonError("Integer exceeds JSON range");
        }
        return static_cast<std::int64_t>(value);
    }
    template <class T>
    const T& get(const char* expected) const {
        if (const auto* value = std::get_if<T>(&value_)) return *value;
        throw JsonError(std::string("Expected JSON ") + expected);
    }
    void dump_to(std::string& out) const;

    std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object> value_;
};
}
