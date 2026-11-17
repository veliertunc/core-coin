#include "corecoin/wallet.hpp"
#include "corecoin/blockchain.hpp"
#include "module.hpp"
#include <stdexcept>
#include <utility>

namespace corecoin {
Wallet::Wallet(std::string address) : address_(std::move(address)) {
    if (!__detail::valid_account(address_)) throw std::invalid_argument("Invalid wallet address");
}
Wallet::Wallet(SigningKey key) : address_(key.address()), key_(std::move(key)) {}
Amount Wallet::balance(const Blockchain& chain) const { return chain.balance(address_); }
Amount Wallet::available_balance(const Blockchain& chain) const { return chain.available_balance(address_); }
std::string Wallet::send(Blockchain& chain, const std::string& recipient, Amount gil) const {
    const auto nonce = chain.next_nonce(address_);
    Transaction transaction(address_, recipient, gil, nonce);
    if (key_) {
        const auto public_key = key_->public_key();
        Transaction unsigned_transaction(address_, recipient, gil, nonce, public_key);
        transaction = Transaction(address_, recipient, gil, nonce, public_key,
                                  key_->sign(unsigned_transaction.signing_payload()));
    }
    const auto id = transaction.id();
    chain.submit(std::move(transaction));
    return id;
}
}
