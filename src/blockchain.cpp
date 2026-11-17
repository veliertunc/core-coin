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
void check_config(ChainConfig config) {
    if (config.difficulty < 1 || config.difficulty > 4 || config.mining_reward <= 0)
        throw std::invalid_argument("Difficulty must be 1..4 and mining reward must be positive");
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
    credit(confirmed_, std::string(initial_wallet_address), initial_wallet_allocation);
    issued_ = initial_wallet_allocation;
    projected_ = confirmed_;
}
void Blockchain::credit(Ledger& ledger, const std::string& account, Amount amount) {
    if (!__detail::valid_account(account) || amount <= 0) throw std::invalid_argument("Invalid recipient or amount");
    auto& target = ledger[account];
    if (target.balance > std::numeric_limits<Amount>::max() - amount)
        throw std::overflow_error("Balance overflow");
    target.balance += amount;
}
void Blockchain::apply_transfer(Ledger& ledger, const Transaction& tx) {
    if (!__detail::valid_account(tx.sender()) || !__detail::valid_account(tx.recipient()) || tx.sender() == tx.recipient())
        throw std::invalid_argument("Transfer requires two distinct valid accounts");
    if (tx.amount() <= 0) throw std::invalid_argument("Amount must be positive");
    auto& sender = ledger[tx.sender()];
    if (sender.nonce != tx.nonce()) throw std::invalid_argument("Incorrect transaction nonce");
    if (sender.nonce == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Account nonce exhausted");
    if (sender.balance < tx.amount()) throw std::invalid_argument("Insufficient balance");
    credit(ledger, tx.recipient(), tx.amount());
    sender.balance -= tx.amount();
    ++sender.nonce;
}
void Blockchain::submit(Transaction transaction) {
    if (pending_.size() >= 1000) throw std::length_error("Pending transaction limit reached");
    auto candidate = projected_;
    apply_transfer(candidate, transaction);
    pending_.push_back(std::move(transaction));
    projected_.swap(candidate);
}
const Block& Blockchain::mine_pending(const std::string& miner) {
    auto candidate = projected_;
    if (!__detail::valid_account(miner)) throw std::invalid_argument("Invalid miner");
    const Amount reward = std::min(config_.mining_reward, maximum_supply - issued_);
    if (reward > 0) credit(candidate, miner, reward);
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
Amount Blockchain::balance(const std::string& account) const {
    const auto it = confirmed_.find(account);
    return it == confirmed_.end() ? 0 : it->second.balance;
}
Amount Blockchain::available_balance(const std::string& account) const {
    const auto it = projected_.find(account);
    return it == projected_.end() ? 0 : it->second.balance;
}
std::uint64_t Blockchain::next_nonce(const std::string& account) const {
    const auto it = projected_.find(account);
    return it == projected_.end() ? 0 : it->second.nonce;
}
bool Blockchain::validate(std::string* error) const { return validate_chain(blocks_, config_, error); }
bool Blockchain::validate_chain(const std::vector<Block>& blocks, ChainConfig config, std::string* error) {
    try {
        __detail::check_config(config);
        if (blocks.empty() || blocks.front().hash() != __detail::genesis().hash() || !blocks.front().has_valid_proof())
            throw std::invalid_argument("Invalid genesis block");
        Ledger ledger;
        credit(ledger, std::string(initial_wallet_address), initial_wallet_allocation);
        Amount issued = initial_wallet_allocation;
        for (std::size_t i = 1; i < blocks.size(); ++i) {
            const auto& block = blocks[i];
            if (block.height() != i || block.previous_hash() != blocks[i-1].hash() ||
                block.timestamp() < blocks[i-1].timestamp() || block.difficulty() != config.difficulty ||
                !block.has_valid_proof()) throw std::invalid_argument("Invalid block header or proof of work");
            const auto& txs = block.transactions();
            if (txs.empty() || txs.size() > 1001) throw std::invalid_argument("Invalid block transaction count");
            for (std::size_t j = 0; j + 1 < txs.size(); ++j) apply_transfer(ledger, txs[j]);
            const auto& reward = txs.back();
            const Amount expected_reward = std::min(config.mining_reward, maximum_supply - issued);
            if (!reward.sender().empty() || !__detail::valid_account(reward.recipient()) ||
                reward.amount() != expected_reward || reward.nonce() != i)
                throw std::invalid_argument("Invalid mining reward");
            if (expected_reward > 0) credit(ledger, reward.recipient(), expected_reward);
            issued += expected_reward;
        }
        if (error) error->clear();
        return true;
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return false;
    }
}
}
