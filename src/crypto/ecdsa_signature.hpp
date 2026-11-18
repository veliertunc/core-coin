#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace corecoin::__detail {
inline constexpr std::size_t compact_signature_bytes = 64;
using CompactSignature = std::array<unsigned char, compact_signature_bytes>;

// Converts a DER ECDSA signature to 32-byte r || 32-byte s, normalized to low S
// so each signature has exactly one valid encoding.
[[nodiscard]] std::optional<CompactSignature> compact_from_der(std::span<const unsigned char> der);
// Converts r || s back to DER; nullopt for zero, out-of-range or high-S values.
[[nodiscard]] std::optional<std::vector<unsigned char>> der_from_compact(std::span<const unsigned char> compact);
}
