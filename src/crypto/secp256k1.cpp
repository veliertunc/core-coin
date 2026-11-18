#include "crypto/secp256k1.hpp"
#include <openssl/core_names.h>
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace corecoin::__detail::secp256k1 {
namespace {
constexpr char order_hex[] = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";
constexpr std::size_t uncompressed_point_bytes = 1 + 2 * scalar_bytes;
}
BigNum order() {
    BIGNUM* value = nullptr;
    if (BN_hex2bn(&value, order_hex) == 0) throw std::runtime_error("Cannot load secp256k1 order");
    return BigNum(value, BN_free);
}
BigNum half_order() {
    auto half = order();
    if (BN_rshift1(half.get(), half.get()) != 1) throw std::runtime_error("Cannot load secp256k1 order");
    return half;
}
PKey generate_key() {
    EVP_PKEY* key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", curve_name);
    if (!key) throw std::runtime_error("ECDSA key generation failed");
    return PKey(key, EVP_PKEY_free);
}
bool is_curve_key(EVP_PKEY* key) {
    std::array<char, 32> name{};
    std::size_t length = 0;
    return EVP_PKEY_base_id(key) == EVP_PKEY_EC &&
        EVP_PKEY_get_group_name(key, name.data(), name.size(), &length) == 1 &&
        std::string_view(name.data(), length) == curve_name;
}
CompressedPoint compressed_public_key(EVP_PKEY* key) {
    // OpenSSL encodes uncompressed by default.
    std::array<unsigned char, uncompressed_point_bytes> point{};
    std::size_t size = 0;
    if (EVP_PKEY_get_octet_string_param(key, OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY, point.data(), point.size(), &size) != 1)
        throw std::runtime_error("Cannot read public key");
    CompressedPoint compressed{};
    if (size == compressed.size()) {
        std::copy_n(point.begin(), compressed.size(), compressed.begin());
        return compressed;
    }
    if (size != point.size() || point[0] != 0x04) throw std::runtime_error("Unexpected public key encoding");
    compressed[0] = static_cast<unsigned char>(0x02 | (point.back() & 1));
    std::copy_n(point.begin() + 1, scalar_bytes, compressed.begin() + 1);
    return compressed;
}
PKey import_public_key(std::span<const unsigned char> point) {
    PKey none(nullptr, EVP_PKEY_free);
    if (point.size() != compressed_point_bytes || (point[0] != 0x02 && point[0] != 0x03)) return none;
    ParamBuilder builder(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!builder ||
        OSSL_PARAM_BLD_push_utf8_string(builder.get(), OSSL_PKEY_PARAM_GROUP_NAME, curve_name, 0) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(builder.get(), OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size()) != 1)
        return none;
    Params params(OSSL_PARAM_BLD_to_param(builder.get()), OSSL_PARAM_free);
    PKeyContext ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* key = nullptr;
    if (!params || !ctx || EVP_PKEY_fromdata_init(ctx.get()) != 1 ||
        EVP_PKEY_fromdata(ctx.get(), &key, EVP_PKEY_PUBLIC_KEY, params.get()) != 1)
        return none;
    return PKey(key, EVP_PKEY_free);
}
}
