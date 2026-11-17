#include "corecoin/blockchain.hpp"
#include "crypto/sha256.hpp"
#include <utility>

namespace corecoin {
namespace {
std::string field(std::string_view value) {
    return std::to_string(value.size()) + ":" + std::string(value);
}
}
Transaction::Transaction(std::string sender, std::string recipient, Amount amount, std::uint64_t nonce,
                         std::string public_key, std::string signature)
    : sender_(std::move(sender)), recipient_(std::move(recipient)), public_key_(std::move(public_key)),
      signature_(std::move(signature)), amount_(amount), nonce_(nonce) {}

std::string Transaction::signing_payload() const {
    return "corecoin-tx-sign-v2|" + field(sender_) + field(recipient_) +
           field(std::to_string(amount_)) + field(std::to_string(nonce_)) +
           field(public_key_);
}

std::string Transaction::serialize() const {
    if (!public_key_.empty() || !signature_.empty())
        return signing_payload() + field(signature_);
    return "corecoin-tx-v1|" + field(sender_) + field(recipient_) +
           field(std::to_string(amount_)) + field(std::to_string(nonce_));
}
std::string Transaction::id() const { return __detail::sha256(serialize()); }

 }
