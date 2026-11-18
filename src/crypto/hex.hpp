#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace corecoin::__detail {
// Lowercase hexadecimal encoding of raw bytes.
[[nodiscard]] std::string to_hex(std::span<const unsigned char> bytes);
// Decodes exactly `expected` bytes of lowercase hex; nullopt for any other input.
[[nodiscard]] std::optional<std::vector<unsigned char>> from_hex(std::string_view text, std::size_t expected);
}
