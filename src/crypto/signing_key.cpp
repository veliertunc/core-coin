#include "corecoin/signing_key.hpp"
#include "crypto/ecdsa_signature.hpp"
#include "crypto/hex.hpp"
#include "crypto/key_address.hpp"
#include "crypto/key_file.hpp"
#include "crypto/secp256k1.hpp"
#include <array>
#include <stdexcept>

namespace corecoin {
struct SigningKey::Impl {
    explicit Impl(__detail::PKey value) : key(std::move(value)) {}
    __detail::PKey key;
};
SigningKey::SigningKey(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
SigningKey SigningKey::generate() {
    return SigningKey(std::make_shared<Impl>(__detail::secp256k1::generate_key()));
}
SigningKey SigningKey::load(const std::filesystem::path& path, std::string_view password) {
    auto key = __detail::read_encrypted_key(path, password);
    if (!__detail::secp256k1::is_curve_key(key.get())) throw std::runtime_error("Wallet key is not an ECDSA secp256k1 key");
    return SigningKey(std::make_shared<Impl>(std::move(key)));
}
void SigningKey::save(const std::filesystem::path& path, std::string_view password) const {
    __detail::write_encrypted_key(impl_->key.get(), path, password);
}
std::string SigningKey::public_key() const {
    return __detail::to_hex(__detail::secp256k1::compressed_public_key(impl_->key.get()));
}
std::string SigningKey::address_for(std::string_view public_key) {
    const auto point = __detail::from_hex(public_key, __detail::secp256k1::compressed_point_bytes);
    if (!point || !__detail::secp256k1::import_public_key(*point)) throw std::invalid_argument("Invalid ECDSA secp256k1 public key");
    return __detail::key_address(public_key);
}
std::string SigningKey::address() const { return address_for(public_key()); }
std::string SigningKey::sign(std::string_view message) const {
    __detail::DigestContext ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::array<unsigned char, 80> der{};  // DER ECDSA over a 256-bit curve is at most 72 bytes
    std::size_t size = der.size();
    if (!ctx || EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, impl_->key.get()) != 1 ||
        EVP_DigestSign(ctx.get(), der.data(), &size, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1)
        throw std::runtime_error("Signing failed");
    const auto compact = __detail::compact_from_der(std::span(der.data(), size));
    if (!compact) throw std::runtime_error("Signing failed");
    return __detail::to_hex(*compact);
}
bool SigningKey::verify(std::string_view public_key, std::string_view message, std::string_view signature) {
    const auto point = __detail::from_hex(public_key, __detail::secp256k1::compressed_point_bytes);
    const auto compact = __detail::from_hex(signature, __detail::compact_signature_bytes);
    if (!point || !compact) return false;
    const auto key = __detail::secp256k1::import_public_key(*point);
    const auto der = __detail::der_from_compact(*compact);
    __detail::DigestContext ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && der && ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1 &&
        EVP_DigestVerify(ctx.get(), der->data(), der->size(), reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
}
}
