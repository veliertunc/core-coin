#include "corecoin/signing_key.hpp"
#include "crypto/sha256.hpp"
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/pem.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace corecoin {
namespace {
template<class T, auto Free> using Handle = std::unique_ptr<T, decltype(Free)>;
using Key = Handle<EVP_PKEY, EVP_PKEY_free>;
using KeyContext = Handle<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;
using Bio = Handle<BIO, BIO_free>;
using Context = Handle<EVP_MD_CTX, EVP_MD_CTX_free>;
using Number = Handle<BIGNUM, BN_free>;
using Signature = Handle<ECDSA_SIG, ECDSA_SIG_free>;
using ParamBuilder = Handle<OSSL_PARAM_BLD, OSSL_PARAM_BLD_free>;
using Params = Handle<OSSL_PARAM, OSSL_PARAM_free>;

constexpr char curve[] = "secp256k1";
constexpr std::size_t public_key_bytes = 33;  // compressed SEC1 point
constexpr std::size_t scalar_bytes = 32;
constexpr std::size_t signature_bytes = 2 * scalar_bytes;  // r || s
// secp256k1 group order n.
constexpr char order_hex[] = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";

std::string hex(const unsigned char* bytes, std::size_t length) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result; result.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
    return result;
}
std::vector<unsigned char> decode(std::string_view input, std::size_t expected) {
    if (input.size() != expected * 2) return {};
    auto digit = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; return -1; };
    std::vector<unsigned char> bytes; bytes.reserve(expected);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const int a = digit(input[i]), b = digit(input[i + 1]);
        if (a < 0 || b < 0) return {};
        bytes.push_back(static_cast<unsigned char>((a << 4) | b));
    }
    return bytes;
}
void check_password(std::string_view password) {
    if (password.size() < 12 || password.size() > 1024 || password.find('\0') != std::string_view::npos)
        throw std::invalid_argument("Password must be 12 to 1024 characters without NUL");
}
int password_callback(char* buffer, int size, int, void* data) {
    const auto password = *static_cast<std::string_view*>(data);
    if (password.size() > static_cast<std::size_t>(size)) return 0;
    std::copy(password.begin(), password.end(), buffer);
    return static_cast<int>(password.size());
}
bool is_secp256k1(EVP_PKEY* key) {
    std::array<char, 32> name{}; std::size_t length = 0;
    return EVP_PKEY_base_id(key) == EVP_PKEY_EC &&
        EVP_PKEY_get_group_name(key, name.data(), name.size(), &length) == 1 &&
        std::string_view(name.data(), length) == curve;
}
Number order() {
    BIGNUM* value = nullptr;
    if (BN_hex2bn(&value, order_hex) == 0) throw std::runtime_error("Cannot load curve order");
    return Number(value, BN_free);
}
// Imports a compressed SEC1 point; OpenSSL rejects points that are not on the curve.
Key public_key_from(const std::vector<unsigned char>& point) {
    ParamBuilder builder(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!builder ||
        OSSL_PARAM_BLD_push_utf8_string(builder.get(), OSSL_PKEY_PARAM_GROUP_NAME, curve, 0) != 1 ||
        OSSL_PARAM_BLD_push_octet_string(builder.get(), OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size()) != 1)
        return Key(nullptr, EVP_PKEY_free);
    Params params(OSSL_PARAM_BLD_to_param(builder.get()), OSSL_PARAM_free);
    KeyContext ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY* key = nullptr;
    if (!params || !ctx || EVP_PKEY_fromdata_init(ctx.get()) != 1 ||
        EVP_PKEY_fromdata(ctx.get(), &key, EVP_PKEY_PUBLIC_KEY, params.get()) != 1)
        return Key(nullptr, EVP_PKEY_free);
    return Key(key, EVP_PKEY_free);
}
bool valid_public_key(std::string_view public_key) {
    const auto point = decode(public_key, public_key_bytes);
    return !point.empty() && (point[0] == 0x02 || point[0] == 0x03) && public_key_from(point);
}
}
struct SigningKey::Impl {
    explicit Impl(EVP_PKEY* value) : key(value, EVP_PKEY_free) {}
    Key key;
};
SigningKey::SigningKey(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
SigningKey SigningKey::generate() {
    EVP_PKEY* key = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", curve);
    if (!key) throw std::runtime_error("ECDSA key generation failed");
    return SigningKey(std::make_shared<Impl>(key));
}
SigningKey SigningKey::load(const std::filesystem::path& path, std::string_view password) {
    check_password(password);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open wallet key file");
    std::string pem((std::istreambuf_iterator<char>(in)), {});
    if (in.bad() || pem.size() > 16384 || pem.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Invalid wallet key file");
    if (!pem.starts_with("-----BEGIN ENCRYPTED PRIVATE KEY-----"))
        throw std::runtime_error("Wallet key file must be encrypted");
    Bio bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    if (!bio) throw std::runtime_error("Cannot read wallet key file");
    Key key(PEM_read_bio_PrivateKey(bio.get(), nullptr, password_callback, &password), EVP_PKEY_free);
    if (!key || !is_secp256k1(key.get())) throw std::runtime_error("Cannot decrypt ECDSA secp256k1 wallet key");
    return SigningKey(std::make_shared<Impl>(key.release()));
}
void SigningKey::save(const std::filesystem::path& path, std::string_view password) const {
    check_password(password);
    Bio bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio || PEM_write_bio_PrivateKey(bio.get(), impl_->key.get(), EVP_aes_256_cbc(), nullptr, 0, password_callback, &password) != 1)
        throw std::runtime_error("Cannot encrypt wallet key");
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio.get(), &data);
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!out) throw std::runtime_error("Cannot create wallet key file (it may already exist)");
    out.write(data, size);
    out.close();
    if (!out) throw std::runtime_error("Cannot write wallet key file");
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
}
std::string SigningKey::public_key() const {
    // OpenSSL encodes uncompressed by default; compress to 0x02/0x03 || x.
    std::array<unsigned char, 65> point{}; std::size_t size = 0;
    if (EVP_PKEY_get_octet_string_param(impl_->key.get(), OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY, point.data(), point.size(), &size) != 1)
        throw std::runtime_error("Cannot read public key");
    if (size == public_key_bytes) return hex(point.data(), size);
    if (size != point.size() || point[0] != 0x04) throw std::runtime_error("Unexpected public key encoding");
    std::array<unsigned char, public_key_bytes> compressed{};
    compressed[0] = static_cast<unsigned char>(0x02 | (point[64] & 1));
    std::copy_n(point.begin() + 1, scalar_bytes, compressed.begin() + 1);
    return hex(compressed.data(), compressed.size());
}
std::string SigningKey::address_for(std::string_view public_key) {
    if (!valid_public_key(public_key)) throw std::invalid_argument("Invalid ECDSA secp256k1 public key");
    return "cc_" + __detail::sha256("corecoin-address-v2|" + std::string(public_key));
}
std::string SigningKey::address() const { return address_for(public_key()); }
std::string SigningKey::sign(std::string_view message) const {
    Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::array<unsigned char, 80> der{}; std::size_t size = der.size();
    if (!ctx || EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, impl_->key.get()) != 1 ||
        EVP_DigestSign(ctx.get(), der.data(), &size, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1)
        throw std::runtime_error("Signing failed");
    const unsigned char* cursor = der.data();
    Signature sig(d2i_ECDSA_SIG(nullptr, &cursor, static_cast<long>(size)), ECDSA_SIG_free);
    if (!sig) throw std::runtime_error("Signing failed");
    const BIGNUM* r = ECDSA_SIG_get0_r(sig.get());
    const BIGNUM* s = ECDSA_SIG_get0_s(sig.get());
    // Normalize to low S so each signature has exactly one valid encoding.
    const auto n = order();
    Number half(BN_dup(n.get()), BN_free), low_s(BN_dup(s), BN_free);
    if (!half || !low_s || BN_rshift1(half.get(), half.get()) != 1 ||
        (BN_cmp(low_s.get(), half.get()) > 0 && BN_sub(low_s.get(), n.get(), s) != 1))
        throw std::runtime_error("Signing failed");
    std::array<unsigned char, signature_bytes> compact{};
    if (BN_bn2binpad(r, compact.data(), scalar_bytes) != static_cast<int>(scalar_bytes) ||
        BN_bn2binpad(low_s.get(), compact.data() + scalar_bytes, scalar_bytes) != static_cast<int>(scalar_bytes))
        throw std::runtime_error("Signing failed");
    return hex(compact.data(), compact.size());
}
bool SigningKey::verify(std::string_view public_key, std::string_view message, std::string_view signature) {
    const auto point = decode(public_key, public_key_bytes), compact = decode(signature, signature_bytes);
    if (point.empty() || compact.empty() || (point[0] != 0x02 && point[0] != 0x03)) return false;
    const auto key = public_key_from(point);
    if (!key) return false;
    Number r(BN_bin2bn(compact.data(), scalar_bytes, nullptr), BN_free);
    Number s(BN_bin2bn(compact.data() + scalar_bytes, scalar_bytes, nullptr), BN_free);
    const auto n = order();
    Number half(BN_dup(n.get()), BN_free);
    if (!r || !s || !half || BN_rshift1(half.get(), half.get()) != 1) return false;
    // Reject zero, out-of-range and high-S values (malleable encodings).
    if (BN_is_zero(r.get()) || BN_is_zero(s.get()) || BN_cmp(r.get(), n.get()) >= 0 || BN_cmp(s.get(), half.get()) > 0) return false;
    Signature sig(ECDSA_SIG_new(), ECDSA_SIG_free);
    if (!sig || ECDSA_SIG_set0(sig.get(), r.get(), s.get()) != 1) return false;
    (void)r.release(); (void)s.release();
    unsigned char* der = nullptr;
    const int der_size = i2d_ECDSA_SIG(sig.get(), &der);
    if (der_size <= 0) return false;
    Handle<unsigned char, [](unsigned char* p) { OPENSSL_free(p); }> der_owner(der, {});
    Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get()) == 1 &&
        EVP_DigestVerify(ctx.get(), der, static_cast<std::size_t>(der_size), reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
}
}
