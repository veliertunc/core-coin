#include "crypto/key_file.hpp"
#include <openssl/pem.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace corecoin::__detail {
namespace {
constexpr std::size_t max_key_file_bytes = 16384;
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
void write_encrypted_key(EVP_PKEY* key, const std::filesystem::path& path, std::string_view password) {
    check_password(password);
    Bio bio(BIO_new(BIO_s_mem()), BIO_free);
    if (!bio || PEM_write_bio_PrivateKey(bio.get(), key, EVP_aes_256_cbc(), nullptr, 0, password_callback, &password) != 1)
        throw std::runtime_error("Cannot encrypt wallet key");
    char* data = nullptr;
    const auto size = BIO_get_mem_data(bio.get(), &data);
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::noreplace);
    if (!out) throw std::runtime_error("Cannot create wallet key file (it may already exist)");
    out.write(data, size);
    out.close();
    if (!out) throw std::runtime_error("Cannot write wallet key file");
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);
}
PKey read_encrypted_key(const std::filesystem::path& path, std::string_view password) {
    check_password(password);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open wallet key file");
    std::string pem((std::istreambuf_iterator<char>(in)), {});
    if (in.bad() || pem.size() > max_key_file_bytes || pem.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Invalid wallet key file");
    if (!pem.starts_with("-----BEGIN ENCRYPTED PRIVATE KEY-----"))
        throw std::runtime_error("Wallet key file must be encrypted");
    Bio bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())), BIO_free);
    if (!bio) throw std::runtime_error("Cannot read wallet key file");
    PKey key(PEM_read_bio_PrivateKey(bio.get(), nullptr, password_callback, &password), EVP_PKEY_free);
    if (!key) throw std::runtime_error("Cannot decrypt wallet key");
    return key;
}
}
