#include "rest/json.hpp"
#include <charconv>
#include <cmath>
#include <limits>

namespace corecoin::__rest {
namespace {
constexpr int max_depth = 32;

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}
    Json document() {
        auto result = value(0);
        whitespace();
        if (pos_ != text_.size()) fail("Unexpected trailing characters");
        return result;
    }

private:
    [[noreturn]] void fail(const char* message) const {
        throw JsonError(std::string(message) + " at offset " + std::to_string(pos_));
    }
    [[nodiscard]] bool at_end() const noexcept { return pos_ >= text_.size(); }
    [[nodiscard]] char peek() const { if (at_end()) fail("Unexpected end of input"); return text_[pos_]; }
    void whitespace() {
        while (!at_end() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
            ++pos_;
    }
    void expect(char c) { if (peek() != c) fail("Unexpected character"); ++pos_; }
    void literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) fail("Invalid literal");
        pos_ += word.size();
    }
    Json value(int depth) {
        if (depth > max_depth) fail("JSON nesting is too deep");
        whitespace();
        switch (peek()) {
        case '{': return object(depth);
        case '[': return array(depth);
        case '"': return Json(string());
        case 't': literal("true"); return Json(true);
        case 'f': literal("false"); return Json(false);
        case 'n': literal("null"); return Json(nullptr);
        default: return number();
        }
    }
    Json object(int depth) {
        expect('{');
        auto result = Json::object();
        whitespace();
        if (peek() == '}') { ++pos_; return result; }
        for (;;) {
            whitespace();
            if (peek() != '"') fail("Expected object key");
            auto key = string();
            if (result.find(key)) fail("Duplicate object key");
            whitespace();
            expect(':');
            result.set(std::move(key), value(depth + 1));
            whitespace();
            if (peek() == '}') { ++pos_; return result; }
            expect(',');
        }
    }
    Json array(int depth) {
        expect('[');
        auto result = Json::array();
        whitespace();
        if (peek() == ']') { ++pos_; return result; }
        for (;;) {
            result.push(value(depth + 1));
            whitespace();
            if (peek() == ']') { ++pos_; return result; }
            expect(',');
        }
    }
    unsigned hex4() {
        if (text_.size() - pos_ < 4) fail("Truncated unicode escape");
        unsigned code = 0;
        const auto [end, error] = std::from_chars(text_.data() + pos_, text_.data() + pos_ + 4, code, 16);
        if (error != std::errc{} || end != text_.data() + pos_ + 4) fail("Invalid unicode escape");
        pos_ += 4;
        return code;
    }
    static void append_utf8(std::string& out, unsigned code) {
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    std::string string() {
        expect('"');
        std::string out;
        for (;;) {
            const char c = peek();
            ++pos_;
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("Control character in string");
            if (c != '\\') { out += c; continue; }
            const char escape = peek();
            ++pos_;
            switch (escape) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                unsigned code = hex4();
                if (code >= 0xD800 && code <= 0xDBFF) {
                    if (text_.substr(pos_, 2) != "\\u") fail("Unpaired surrogate");
                    pos_ += 2;
                    const unsigned low = hex4();
                    if (low < 0xDC00 || low > 0xDFFF) fail("Invalid surrogate pair");
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                } else if (code >= 0xDC00 && code <= 0xDFFF) fail("Unpaired surrogate");
                append_utf8(out, code);
                break;
            }
            default: fail("Invalid escape sequence");
            }
        }
    }
    Json number() {
        const auto start = pos_;
        bool integral = true;
        if (!at_end() && text_[pos_] == '-') ++pos_;
        const auto digits = [&] {
            const auto first = pos_;
            while (!at_end() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            return pos_ - first;
        };
        const auto int_start = pos_;
        const auto int_digits = digits();
        if (int_digits == 0) fail("Invalid value");
        if (int_digits > 1 && text_[int_start] == '0') fail("Leading zeros are not allowed");
        if (!at_end() && text_[pos_] == '.') {
            integral = false; ++pos_;
            if (digits() == 0) fail("Invalid fraction");
        }
        if (!at_end() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            integral = false; ++pos_;
            if (!at_end() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (digits() == 0) fail("Invalid exponent");
        }
        const auto token = text_.substr(start, pos_ - start);
        if (integral) {
            std::int64_t value{};
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
            if (error == std::errc{} && end == token.data() + token.size()) return Json(value);
        }
        double value{};
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (error != std::errc{} || end != token.data() + token.size()) fail("Number out of range");
        return Json(value);
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

void escape_string(std::string& out, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (const char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else out += c;
        }
    }
    out += '"';
}
}

Json Json::parse(std::string_view text) { return Parser(text).document(); }

const Json* Json::find(std::string_view key) const {
    const auto* members = std::get_if<Object>(&value_);
    if (!members) return nullptr;
    for (const auto& [name, value] : *members)
        if (name == key) return &value;
    return nullptr;
}
Json& Json::set(std::string key, Json value) {
    auto* members = std::get_if<Object>(&value_);
    if (!members) throw JsonError("Expected JSON object");
    for (auto& [name, existing] : *members)
        if (name == key) { existing = std::move(value); return *this; }
    members->emplace_back(std::move(key), std::move(value));
    return *this;
}
Json& Json::push(Json value) {
    auto* items = std::get_if<Array>(&value_);
    if (!items) throw JsonError("Expected JSON array");
    items->push_back(std::move(value));
    return *this;
}
std::string Json::dump() const {
    std::string out;
    dump_to(out);
    return out;
}
void Json::dump_to(std::string& out) const {
    std::visit([&out](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) out += "null";
        else if constexpr (std::is_same_v<T, bool>) out += value ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::int64_t>) out += std::to_string(value);
        else if constexpr (std::is_same_v<T, double>) {
            if (!std::isfinite(value)) { out += "null"; return; }
            char buffer[32];
            const auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, value);
            out.append(buffer, error == std::errc{} ? end : buffer);
        } else if constexpr (std::is_same_v<T, std::string>) escape_string(out, value);
        else if constexpr (std::is_same_v<T, Array>) {
            out += '[';
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i) out += ',';
                value[i].dump_to(out);
            }
            out += ']';
        } else {
            out += '{';
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i) out += ',';
                escape_string(out, value[i].first);
                out += ':';
                value[i].second.dump_to(out);
            }
            out += '}';
        }
    }, value_);
}
}
