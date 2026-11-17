#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace corecoin {
// All ledger amounts are integer Gil, CoreCoin's smallest indivisible unit.
using Amount = std::int64_t;
inline constexpr Amount gil_per_coin = 100'000'000;
inline constexpr Amount units_per_coin = gil_per_coin; // Backward-compatible alias.

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
    Amount mining_reward = 50 * gil_per_coin;
};

class Blockchain {
public:
    explicit Blockchain(ChainConfig config = {});
    void submit(Transaction transaction);
    const Block& mine_pending(const std::string& miner);
    [[nodiscard]] Amount balance(const std::string& account) const;
    // Includes queued transactions, allowing multiple sequential submissions.
    [[nodiscard]] std::uint64_t next_nonce(const std::string& account) const;
    [[nodiscard]] bool validate(std::string* error = nullptr) const;
    [[nodiscard]] static bool validate_chain(const std::vector<Block>& blocks,
                                           ChainConfig config, std::string* error = nullptr);
    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return blocks_; }
    [[nodiscard]] const std::vector<Transaction>& pending() const noexcept { return pending_; }
private:
    struct Account { Amount balance = 0; std::uint64_t nonce = 0; };
    using Ledger = std::map<std::string, Account>;
    static void apply_transfer(Ledger& ledger, const Transaction& transaction);
    static void credit(Ledger& ledger, const std::string& account, Amount amount);
    ChainConfig config_;
    std::vector<Block> blocks_;
    std::vector<Transaction> pending_;
    Ledger confirmed_, projected_;
};

[[nodiscard]] std::string format_amount(Amount amount);
}
