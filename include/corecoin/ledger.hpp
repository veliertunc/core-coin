#pragma once
#include "amount.hpp"
#include <cstdint>
#include <map>
#include <string>

namespace corecoin {
class Transaction;

// Account balances and transaction sequence numbers for one ledger state.
class Ledger {
public:
    void credit(const std::string& account, Amount amount);
    void apply_transfer(const Transaction& transaction);
    [[nodiscard]] Amount balance(const std::string& account) const;
    [[nodiscard]] std::uint64_t next_nonce(const std::string& account) const;
    void swap(Ledger& other) noexcept;
private:
    struct Account { Amount balance = 0; std::uint64_t nonce = 0; };
    std::map<std::string, Account> accounts_;
};
}
