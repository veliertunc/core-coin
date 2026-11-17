#include "corecoin/ledger.hpp"
#include "corecoin/blockchain.hpp"
#include "address.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace corecoin {
void Ledger::credit( const std::string& account, Amount amount) {
    if (!__detail::valid_account(account) || amount <= 0) throw std::invalid_argument("Invalid recipient or amount");
    auto& target = accounts_[account];
    if (target.balance > std::numeric_limits<Amount>::max() - amount)
        throw std::overflow_error("Balance overflow");
    target.balance += amount;
}
void Ledger::apply_transfer( const Transaction& tx) {
    if (!__detail::valid_account(tx.sender()) || !__detail::valid_account(tx.recipient()) || tx.sender() == tx.recipient())
        throw std::invalid_argument("Transfer requires two distinct valid accounts");
    if (tx.amount() <= 0) throw std::invalid_argument("Amount must be positive");
    auto& sender = accounts_[tx.sender()];
    if (sender.nonce != tx.nonce()) throw std::invalid_argument("Incorrect transaction nonce");
    if (sender.nonce == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Account nonce exhausted");
    if (sender.balance < tx.amount()) throw std::invalid_argument("Insufficient balance");
    credit(tx.recipient(), tx.amount());
    sender.balance -= tx.amount();
    ++sender.nonce;
}
Amount Ledger::balance(const std::string& account) const {
    const auto it = accounts_.find(account);
    return it == accounts_.end() ? 0 : it->second.balance;
}
std::uint64_t Ledger::next_nonce(const std::string& account) const {
    const auto it = accounts_.find(account);
    return it == accounts_.end() ? 0 : it->second.nonce;
}
void Ledger::swap(Ledger& other) noexcept { accounts_.swap(other.accounts_); }
}
