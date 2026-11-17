#pragma once
#include "amount.hpp"
#include "signing_key.hpp"
#include <optional>
#include <string>

namespace corecoin {
class Blockchain;

// Named accounts are legacy accounts; key-backed wallets sign their transfers.
class Wallet {
public:
    explicit Wallet(std::string address);
    explicit Wallet(SigningKey key);
    [[nodiscard]] const std::string& address() const noexcept { return address_; }
    [[nodiscard]] Amount balance(const Blockchain& chain) const;
    [[nodiscard]] Amount available_balance(const Blockchain& chain) const;
    // Queues a transfer; the recipient receives confirmed funds after mining.
    std::string send(Blockchain& chain, const std::string& recipient, Amount gil) const;
private:
    std::string address_;
    std::optional<SigningKey> key_;
};
}
