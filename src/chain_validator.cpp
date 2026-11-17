#include "chain_validator.hpp"
#include "address.hpp"
#include "genesis.hpp"
#include "corecoin/network.hpp"
#include <algorithm>
#include <stdexcept>

namespace corecoin::__detail {
void check_config(ChainConfig config) {
    if (config.difficulty < 1 || config.difficulty > 4 || config.mining_reward <= 0)
        throw std::invalid_argument("Difficulty must be 1..4 and mining reward must be positive");
}
bool ChainValidator::validate(const std::vector<Block>& blocks, ChainConfig config, std::string* error) {
    try {
        check_config(config);
        if (blocks.empty() || blocks.front().hash() != genesis().hash() || !blocks.front().has_valid_proof())
            throw std::invalid_argument("Invalid genesis block");
        Ledger ledger;
        ledger.credit(std::string(initial_wallet_address), initial_wallet_allocation);
        Amount issued = initial_wallet_allocation;
        for (std::size_t i = 1; i < blocks.size(); ++i) {
            const auto& block = blocks[i];
            if (block.height() != i || block.previous_hash() != blocks[i-1].hash() ||
                block.timestamp() < blocks[i-1].timestamp() || block.difficulty() != config.difficulty ||
                !block.has_valid_proof()) throw std::invalid_argument("Invalid block header or proof of work");
            const auto& txs = block.transactions();
            if (txs.empty() || txs.size() > 1001) throw std::invalid_argument("Invalid block transaction count");
            for (std::size_t j = 0; j + 1 < txs.size(); ++j) ledger.apply_transfer(txs[j]);
            const auto& reward = txs.back();
            const Amount expected_reward = std::min(config.mining_reward, maximum_supply - issued);
            if (!reward.sender().empty() || !valid_account(reward.recipient()) ||
                reward.amount() != expected_reward || reward.nonce() != i ||
                !reward.public_key().empty() || !reward.signature().empty())
                throw std::invalid_argument("Invalid mining reward");
            if (expected_reward > 0) ledger.credit(reward.recipient(), expected_reward);
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
