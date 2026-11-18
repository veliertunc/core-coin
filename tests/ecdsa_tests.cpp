#include "module.hpp"
#include "../src/module.hpp"
#include <string>
#include <string_view>

namespace corecoin::__tests {
namespace {
using namespace corecoin::__detail;
constexpr std::string_view order_hex = "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141";
constexpr std::string_view generator = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798";

// a - b for equal-length lowercase hex numbers with a >= b.
std::string hex_subtract(std::string_view a, std::string_view b) {
    auto value = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    std::string result(a.size(), '0');
    int borrow = 0;
    for (std::size_t i = a.size(); i-- > 0;) {
        int digit = value(a[i]) - value(b[i]) - borrow;
        borrow = digit < 0;
        if (borrow) digit += 16;
        result[i] = "0123456789abcdef"[digit];
    }
    return result;
}
std::vector<unsigned char> bytes(std::string_view hex, std::size_t size) {
    auto decoded = from_hex(hex, size);
    if (!decoded) throw std::invalid_argument("Bad test vector");
    return *decoded;
}
}

TEST(HexTests, RoundTripsAndRejectsMalformedInput) {
    const std::vector<unsigned char> raw{0x00, 0x7f, 0x80, 0xff};
    EXPECT_EQ(to_hex(raw), "007f80ff");
    EXPECT_EQ(from_hex("007f80ff", 4), raw);
    EXPECT_FALSE(from_hex("007f80", 4));
    EXPECT_FALSE(from_hex("007F80FF", 4)) << "Uppercase is not canonical";
    EXPECT_FALSE(from_hex("007g80ff", 4));
}

TEST(EcdsaSignatureTests, CompactEncodingRejectsOutOfRangeScalars) {
    const std::string one = std::string(63, '0') + "1";
    const auto highest_s = hex_subtract(order_hex, std::string(63, '0') + "1");  // n - 1
    EXPECT_TRUE(der_from_compact(bytes(one + one, 64)));
    EXPECT_FALSE(der_from_compact(bytes(std::string(64, '0') + one, 64))) << "Zero r";
    EXPECT_FALSE(der_from_compact(bytes(one + std::string(64, '0'), 64))) << "Zero s";
    EXPECT_FALSE(der_from_compact(bytes(std::string(order_hex) + one, 64))) << "r = n";
    EXPECT_FALSE(der_from_compact(bytes(one + highest_s, 64))) << "High S";
    EXPECT_FALSE(der_from_compact(bytes(one, 32))) << "Wrong length";
}

TEST(EcdsaSignatureTests, DerRoundTripNormalizesToLowS) {
    const std::string r = std::string(63, '0') + "5";
    const std::string low_s = std::string(63, '0') + "7";
    const auto der = der_from_compact(bytes(r + low_s, 64));
    ASSERT_TRUE(der);
    const auto compact = compact_from_der(*der);
    ASSERT_TRUE(compact);
    EXPECT_EQ(to_hex(*compact), r + low_s);
    std::vector<unsigned char> trailing = *der;
    trailing.push_back(0);
    EXPECT_FALSE(compact_from_der(trailing)) << "Trailing bytes after DER";
}

TEST(SigningKeyTests, AddressDerivesFromCompressedEcdsaKey) {
    const auto key = SigningKey::generate();
    const auto public_key = key.public_key();
    EXPECT_EQ(SigningKey::algorithm, "ECDSA-secp256k1-SHA256");
    EXPECT_EQ(public_key.size(), 66u);
    EXPECT_TRUE(public_key.starts_with("02") || public_key.starts_with("03"));
    EXPECT_EQ(key.address(), SigningKey::address_for(public_key));
    EXPECT_EQ(key.address(), key_address(public_key));
    EXPECT_EQ(key_address(public_key), "cc_" + sha256("corecoin-address-v2|" + public_key));
    EXPECT_EQ(SigningKey::address_for(generator).size(), 67u);
    EXPECT_THROW((void)SigningKey::address_for("02" + std::string(64, '0')), std::invalid_argument) << "Not on curve";
    EXPECT_THROW((void)SigningKey::address_for("04" + std::string(128, 'a')), std::invalid_argument) << "Uncompressed";
    EXPECT_THROW((void)SigningKey::address_for(std::string(generator).replace(0, 2, "05")), std::invalid_argument) << "Bad prefix";
}

TEST(SigningKeyTests, SignaturesAreCompactAndLowSOnly) {
    const auto key = SigningKey::generate();
    for (int i = 0; i < 16; ++i) {
        const auto message = "transfer-" + std::to_string(i);
        const auto signature = key.sign(message);
        ASSERT_EQ(signature.size(), 128u);
        EXPECT_TRUE(SigningKey::verify(key.public_key(), message, signature));
        // (r, n - s) is mathematically valid ECDSA, but only the low-S form is accepted.
        const auto high_s = hex_subtract(order_hex, signature.substr(64));
        EXPECT_FALSE(SigningKey::verify(key.public_key(), message, signature.substr(0, 64) + high_s));
    }
}
}
