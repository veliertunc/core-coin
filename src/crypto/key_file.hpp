#pragma once
#include "crypto/openssl.hpp"
#include <filesystem>
#include <string_view>

namespace corecoin::__detail {
// Password-encrypted PKCS#8 PEM storage for private keys. Passwords must be
// 12 to 1024 characters without NUL. Writing never overwrites an existing file
// and restricts it to the owner.
void write_encrypted_key(EVP_PKEY* key, const std::filesystem::path& path, std::string_view password);
[[nodiscard]] PKey read_encrypted_key(const std::filesystem::path& path, std::string_view password);
}
