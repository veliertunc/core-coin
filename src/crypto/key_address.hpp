#pragma once
#include <string>
#include <string_view>

namespace corecoin::__detail {
// Key-backed account address: "cc_" + sha256("corecoin-address-v2|" + public_key_hex).
// The caller is responsible for validating the public key.
[[nodiscard]] std::string key_address(std::string_view public_key_hex);
}
