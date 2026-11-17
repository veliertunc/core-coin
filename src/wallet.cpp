#include "corecoin/wallet.hpp"
#include "corecoin/blockchain.hpp"
#include "module.hpp"
#include <stdexcept>
#include <utility>

namespace corecoin {
Wallet::Wallet(std::string address) : address_(std::move(address)) {
    if (!__detail::valid_account(address_)) throw std::invalid_argument("Invalid wallet address");
}
Amount Wallet::balance(const Blockchain& chain) const { return chain.balance(address_); }
Amount Wallet::available_balance(const Blockchain& chain) const { return chain.available_balance(address_); }
std::string Wallet::send(Blockchain& chain, const std::string& recipient, Amount gil) const {
    Transaction transaction(address_, recipient, gil, chain.next_nonce(address_));
    const auto id = transaction.id();
    chain.submit(std::move(transaction));
    return id;
}
}
