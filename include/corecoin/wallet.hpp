#pragma once
#include "amount.hpp"
#include <string>

namespace corecoin {
class Blockchain;

// Local account facade. Addresses are names, not cryptographic keys.
class Wallet {
public:
    explicit Wallet(std::string address);
    [[nodiscard]] const std::string& address() const noexcept { return address_; }
    [[nodiscard]] Amount balance(const Blockchain& chain) const;
    [[nodiscard]] Amount available_balance(const Blockchain& chain) const;
    // Queues a transfer; the recipient receives confirmed funds after mining.
    std::string send(Blockchain& chain, const std::string& recipient, Amount gil) const;
private:
    std::string address_;
};
}
