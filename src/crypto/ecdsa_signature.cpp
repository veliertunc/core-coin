#include "crypto/ecdsa_signature.hpp"
#include "crypto/secp256k1.hpp"

namespace corecoin::__detail {
namespace {
constexpr int scalar_bytes = static_cast<int>(secp256k1::scalar_bytes);
}
std::optional<CompactSignature> compact_from_der(std::span<const unsigned char> der) {
    const unsigned char* cursor = der.data();
    EcdsaSig sig(d2i_ECDSA_SIG(nullptr, &cursor, static_cast<long>(der.size())), ECDSA_SIG_free);
    if (!sig || cursor != der.data() + der.size()) return std::nullopt;
    const BIGNUM* r = ECDSA_SIG_get0_r(sig.get());
    const BIGNUM* s = ECDSA_SIG_get0_s(sig.get());
    const auto n = secp256k1::order();
    const auto half = secp256k1::half_order();
    BigNum low_s(BN_dup(s), BN_free);
    if (!low_s || (BN_cmp(low_s.get(), half.get()) > 0 && BN_sub(low_s.get(), n.get(), s) != 1)) return std::nullopt;
    CompactSignature compact{};
    if (BN_bn2binpad(r, compact.data(), scalar_bytes) != scalar_bytes ||
        BN_bn2binpad(low_s.get(), compact.data() + scalar_bytes, scalar_bytes) != scalar_bytes)
        return std::nullopt;
    return compact;
}
std::optional<std::vector<unsigned char>> der_from_compact(std::span<const unsigned char> compact) {
    if (compact.size() != compact_signature_bytes) return std::nullopt;
    BigNum r(BN_bin2bn(compact.data(), scalar_bytes, nullptr), BN_free);
    BigNum s(BN_bin2bn(compact.data() + scalar_bytes, scalar_bytes, nullptr), BN_free);
    if (!r || !s) return std::nullopt;
    const auto n = secp256k1::order();
    const auto half = secp256k1::half_order();
    if (BN_is_zero(r.get()) || BN_is_zero(s.get()) || BN_cmp(r.get(), n.get()) >= 0 || BN_cmp(s.get(), half.get()) > 0)
        return std::nullopt;
    EcdsaSig sig(ECDSA_SIG_new(), ECDSA_SIG_free);
    if (!sig || ECDSA_SIG_set0(sig.get(), r.get(), s.get()) != 1) return std::nullopt;
    (void)r.release();  // now owned by sig
    (void)s.release();
    const int size = i2d_ECDSA_SIG(sig.get(), nullptr);
    if (size <= 0) return std::nullopt;
    std::vector<unsigned char> der(static_cast<std::size_t>(size));
    unsigned char* out = der.data();
    if (i2d_ECDSA_SIG(sig.get(), &out) != size) return std::nullopt;
    return der;
}
}
