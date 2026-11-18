#include "crypto/hex.hpp"

namespace corecoin::__detail {
std::string to_hex(std::span<const unsigned char> bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}
std::optional<std::vector<unsigned char>> from_hex(std::string_view text, std::size_t expected) {
    if (text.size() != expected * 2) return std::nullopt;
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::vector<unsigned char> bytes;
    bytes.reserve(expected);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = digit(text[i]), low = digit(text[i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return bytes;
}
}
