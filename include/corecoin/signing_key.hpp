#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace corecoin {
// Ed25519 key pair. Key files are encrypted; keep a separate backup of the file.
class SigningKey {
public:
    static SigningKey generate();
    static SigningKey load(const std::filesystem::path& path, std::string_view password);
    void save(const std::filesystem::path& path, std::string_view password) const;
    [[nodiscard]] std::string public_key() const;
    [[nodiscard]] std::string address() const;
    [[nodiscard]] static std::string address_for(std::string_view public_key);
    [[nodiscard]] std::string sign(std::string_view message) const;
    [[nodiscard]] static bool verify(std::string_view public_key, std::string_view message, std::string_view signature);
private:
    struct Impl;
    explicit SigningKey(std::shared_ptr<Impl> impl);
    std::shared_ptr<Impl> impl_;
};
}
