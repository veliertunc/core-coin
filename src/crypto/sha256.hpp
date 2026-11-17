#pragma once
#include <string>
#include <string_view>

namespace corecoin::__detail {
// Lowercase hexadecimal SHA-256 digest of the input bytes.
[[nodiscard]] std::string sha256(std::string_view input);
}
