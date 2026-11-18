#pragma once
#include "crypto/openssl.hpp"
#include <array>
#include <cstddef>
#include <span>

namespace corecoin::__detail::secp256k1 {
inline constexpr char curve_name[] = "secp256k1";
inline constexpr std::size_t scalar_bytes = 32;
inline constexpr std::size_t compressed_point_bytes = 1 + scalar_bytes;
using CompressedPoint = std::array<unsigned char, compressed_point_bytes>;

// Group order n.
[[nodiscard]] BigNum order();
// Upper bound for low-S signatures: floor(n / 2).
[[nodiscard]] BigNum half_order();
[[nodiscard]] PKey generate_key();
[[nodiscard]] bool is_curve_key(EVP_PKEY* key);
// SEC1 compressed encoding (0x02/0x03 || x) of the key's public point.
[[nodiscard]] CompressedPoint compressed_public_key(EVP_PKEY* key);
// Imports a compressed SEC1 point; null if malformed or not on the curve.
[[nodiscard]] PKey import_public_key(std::span<const unsigned char> point);
}
