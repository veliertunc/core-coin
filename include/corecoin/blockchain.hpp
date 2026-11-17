#pragma once
#include "amount.hpp"
#include "ledger.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace corecoin {
class Transaction {
public:
    Transaction(std::string sender, std::string recipient, Amount amount, std::uint64_t nonce);
    [[nodiscard]] const std::string& sender() const noexcept { return sender_; }
    [[nodiscard]] const std::string& recipient() const noexcept { return recipient_; }
    [[nodiscard]] Amount amount() const noexcept { return amount_; }
    [[nodiscard]] std::uint64_t nonce() const noexcept { return nonce_; }
    [[nodiscard]] std::string serialize() const;
    [[nodiscard]] std::string id() const;
private:
    std::string sender_, recipient_;
    Amount amount_;
    std::uint64_t nonce_;
};

class Block {
public:
    Block(std::uint64_t height, std::string previous_hash, std::int64_t timestamp,
          unsigned difficulty, std::vector<Transaction> transactions);
    void mine(std::uint64_t max_attempts = 10'000'000);
    [[nodiscard]] std::string calculate_hash() const;
    [[nodiscard]] bool has_valid_proof() const;
    [[nodiscard]] std::uint64_t height() const noexcept { return height_; }
    [[nodiscard]] const std::string& previous_hash() const noexcept { return previous_hash_; }
    [[nodiscard]] const std::string& hash() const noexcept { return hash_; }
    [[nodiscard]] std::int64_t timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] unsigned difficulty() const noexcept { return difficulty_; }
    [[nodiscard]] std::uint64_t nonce() const noexcept { return nonce_; }
    [[nodiscard]] const std::vector<Transaction>& transactions() const noexcept { return transactions_; }
private:
    std::uint64_t height_, nonce_ = 0;
    std::string previous_hash_, hash_;
    std::int64_t timestamp_;
    unsigned difficulty_;
    std::vector<Transaction> transactions_;
};

struct ChainConfig {
    unsigned difficulty = 3; // Number of leading zero hexadecimal digits; demo range 1..4.
    Amount mining_reward = 11 * gil_per_coin;
};

class Blockchain {
public:
    explicit Blockchain(ChainConfig config = {});
    void submit(Transaction transaction);
    const Block& mine_pending(const std::string& miner);
    [[nodiscard]] Amount balance(const std::string& account) const;
    [[nodiscard]] Amount available_balance(const std::string& account) const;
    // Includes queued transactions, allowing multiple sequential submissions.
    [[nodiscard]] std::uint64_t next_nonce(const std::string& account) const;
    [[nodiscard]] bool validate(std::string* error = nullptr) const;
    [[nodiscard]] static bool validate_chain(const std::vector<Block>& blocks,
                                           ChainConfig config, std::string* error = nullptr);
    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] const std::vector<Transaction>& pending() const noexcept { return pending_; }
private:
    ChainConfig config_;
    std::vector<Block> blocks_;
    std::vector<Transaction> pending_;
    Ledger confirmed_, projected_;
    Amount issued_ = 0;
};

}
