#include "corecoin/signing_key.hpp"
#include "crypto/sha256.hpp"
#include <openssl/evp.h>
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
using Bio = Handle<BIO, BIO_free>;
using Context = Handle<EVP_MD_CTX, EVP_MD_CTX_free>;
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
}
struct SigningKey::Impl {
    explicit Impl(EVP_PKEY* value) : key(value, EVP_PKEY_free) {}
    Key key;
};
SigningKey::SigningKey(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}
SigningKey SigningKey::generate() {
    EVP_PKEY* key = EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519");
    if (!key) throw std::runtime_error("Ed25519 key generation failed");
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
    if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_ED25519) throw std::runtime_error("Cannot decrypt Ed25519 wallet key");
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
    std::array<unsigned char, 32> bytes{}; std::size_t size = bytes.size();
    if (EVP_PKEY_get_raw_public_key(impl_->key.get(), bytes.data(), &size) != 1 || size != bytes.size()) throw std::runtime_error("Cannot read public key");
    return hex(bytes.data(), size);
}
std::string SigningKey::address_for(std::string_view public_key) {
    if (decode(public_key, 32).empty()) throw std::invalid_argument("Invalid Ed25519 public key");
    return "cc_" + __detail::sha256("corecoin-address-v1|" + std::string(public_key));
}
std::string SigningKey::address() const { return address_for(public_key()); }
std::string SigningKey::sign(std::string_view message) const {
    Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free); std::array<unsigned char, 64> sig{}; std::size_t size = sig.size();
    if (!ctx || EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, impl_->key.get()) != 1 ||
        EVP_DigestSign(ctx.get(), sig.data(), &size, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1)
        throw std::runtime_error("Signing failed");
    return hex(sig.data(), size);
}
bool SigningKey::verify(std::string_view public_key, std::string_view message, std::string_view signature) {
    const auto key_bytes = decode(public_key, 32), sig = decode(signature, 64);
    if (key_bytes.empty() || sig.empty()) return false;
    Key key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, key_bytes.data(), key_bytes.size()), EVP_PKEY_free);
    Context ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    return key && ctx && EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, key.get()) == 1 &&
        EVP_DigestVerify(ctx.get(), sig.data(), sig.size(), reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
}
}
