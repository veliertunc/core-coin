#include "corecoin/blockchain.hpp"
#include "corecoin/network.hpp"
#include "module.hpp"
#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace corecoin {
namespace __detail {
std::string field(std::string_view value) {
    return std::to_string(value.size()) + ":" + std::string(value);
}
}

Transaction::Transaction(std::string sender, std::string recipient, Amount amount, std::uint64_t nonce)
    : sender_(std::move(sender)), recipient_(std::move(recipient)), amount_(amount), nonce_(nonce) {}

std::string Transaction::serialize() const {
    return "corecoin-tx-v1|" + __detail::field(sender_) + __detail::field(recipient_) +
           __detail::field(std::to_string(amount_)) + __detail::field(std::to_string(nonce_));
}
std::string Transaction::id() const { return __detail::sha256(serialize()); }

Block::Block(std::uint64_t height, std::string previous_hash, std::int64_t timestamp,
             unsigned difficulty, std::vector<Transaction> transactions)
    : height_(height), previous_hash_(std::move(previous_hash)), timestamp_(timestamp),
      difficulty_(difficulty), transactions_(std::move(transactions)) {
    if (difficulty > 4) throw std::invalid_argument("Demo difficulty cannot exceed 4");
    hash_ = calculate_hash();
}
std::string Block::calculate_hash() const {
    std::string header = "corecoin-block-v1|" + __detail::field(std::to_string(height_)) +
        __detail::field(previous_hash_) + __detail::field(std::to_string(timestamp_)) +
        __detail::field(std::to_string(difficulty_)) + __detail::field(std::to_string(nonce_)) +
        __detail::field(std::to_string(transactions_.size()));
    for (const auto& tx : transactions_) header += __detail::field(tx.serialize());
    return __detail::sha256(header);
}
bool Block::has_valid_proof() const {
    return hash_ == calculate_hash() && hash_.starts_with(std::string(difficulty_, '0'));
}
void Block::mine(std::uint64_t max_attempts) {
    for (std::uint64_t attempt = 0; attempt < max_attempts; ++attempt) {
        hash_ = calculate_hash();
        if (hash_.starts_with(std::string(difficulty_, '0'))) return;
        if (nonce_ == std::numeric_limits<std::uint64_t>::max()) break;
        ++nonce_;
    }
    throw std::runtime_error("Mining attempt budget exhausted");
}

Blockchain::Blockchain(ChainConfig config) : config_(config) {
    __detail::check_config(config_);
    blocks_.push_back(__detail::genesis());
    confirmed_.credit(std::string(initial_wallet_address), initial_wallet_allocation);
    issued_ = initial_wallet_allocation;
    projected_ = confirmed_;
}
void Blockchain::submit(Transaction transaction) {
    if (pending_.size() >= 1000) throw std::length_error("Pending transaction limit reached");
    auto candidate = projected_;
    candidate.apply_transfer(transaction);
    pending_.push_back(std::move(transaction));
    projected_.swap(candidate);
}
const Block& Blockchain::mine_pending(const std::string& miner) {
    auto candidate = projected_;
    if (!__detail::valid_account(miner)) throw std::invalid_argument("Invalid miner");
    const Amount reward = std::min(config_.mining_reward, maximum_supply - issued_);
    if (reward > 0) candidate.credit(miner, reward);
    auto transactions = pending_;
    // Empty sender denotes coinbase; transfers cannot use it. Height prevents identical rewards.
    transactions.emplace_back("", miner, reward, blocks_.size());
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    Block block(blocks_.size(), blocks_.back().hash(), std::max<std::int64_t>(now, blocks_.back().timestamp()),
                config_.difficulty, std::move(transactions));
    block.mine();
    auto projected = candidate;
    blocks_.push_back(std::move(block));
    issued_ += reward;
    confirmed_.swap(candidate);
    projected_.swap(projected);
    pending_.clear();
    return blocks_.back();
}
Amount Blockchain::balance(const std::string& account) const { return confirmed_.balance(account); }
Amount Blockchain::available_balance(const std::string& account) const { return projected_.balance(account); }
std::uint64_t Blockchain::next_nonce(const std::string& account) const { return projected_.next_nonce(account); }
bool Blockchain::validate(std::string* error) const { return validate_chain(blocks_, config_, error); }
bool Blockchain::validate_chain(const std::vector<Block>& blocks, ChainConfig config, std::string* error) {
    return __detail::ChainValidator::validate(blocks, config, error);
}
}
