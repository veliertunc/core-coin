#include "crypto/key_address.hpp"
#include "crypto/sha256.hpp"

namespace corecoin::__detail {
std::string key_address(std::string_view public_key_hex) {
    return "cc_" + sha256("corecoin-address-v2|" + std::string(public_key_hex));
}
}
