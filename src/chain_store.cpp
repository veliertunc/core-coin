#include "corecoin/chain_store.hpp"
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <utility>

namespace corecoin {
ChainStore::ChainStore(std::filesystem::path path) : path_(std::move(path)) {
    if (path_.empty()) throw std::invalid_argument("Empty chain path");
}
bool ChainStore::exists() const {
    auto backup = path_; backup += ".bak";
    if (std::filesystem::exists(backup))
        throw std::runtime_error("Chain backup exists; recover or remove it before opening the chain");
    return std::filesystem::exists(path_);
}
Blockchain ChainStore::load() const {
    std::ifstream in(path_, std::ios::binary);
    std::string magic;
    ChainConfig config;
    std::size_t count;
    if (!(in >> magic >> config.difficulty >> config.mining_reward >> count) || magic != "CORECOIN1" || count == 0 || count > 1000000)
        throw std::runtime_error("Invalid chain file header");
    std::vector<Block> blocks;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t height, nonce;
        std::int64_t timestamp;
        unsigned difficulty;
        std::size_t tx_count;
        std::string previous;
        if (!(in >> height >> previous >> timestamp >> difficulty >> nonce >> tx_count) || previous.size() != 64 || tx_count > 1001)
            throw std::runtime_error("Invalid block in chain file");
        std::vector<Transaction> transactions;
        for (std::size_t j = 0; j < tx_count; ++j) {
            std::string sender, recipient;
            Amount amount;
            std::uint64_t tx_nonce;
            if (!(in >> std::quoted(sender) >> std::quoted(recipient) >> amount >> tx_nonce) || sender.size() > 64 || recipient.size() > 64)
                throw std::runtime_error("Invalid transaction in chain file");
            transactions.emplace_back(std::move(sender), std::move(recipient), amount, tx_nonce);
        }
        blocks.emplace_back(height, std::move(previous), timestamp, difficulty, std::move(transactions), nonce);
    }
    in >> std::ws;
    if (!in.eof()) throw std::runtime_error("Trailing data in chain file");
    return Blockchain::from_blocks(std::move(blocks), config);
}
void ChainStore::save(const Blockchain& chain) const {
    std::string error;
    if (!chain.validate(&error)) throw std::runtime_error("Cannot save invalid chain: " + error);
    auto temp = path_; temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot open temporary chain file");
        const auto config = chain.config();
        out << "CORECOIN1 " << config.difficulty << ' ' << config.mining_reward << ' ' << chain.blocks().size() << '\n';
        for (const auto& block : chain.blocks()) {
            out << block.height() << ' ' << block.previous_hash() << ' ' << block.timestamp() << ' ' << block.difficulty() << ' ' << block.nonce() << ' ' << block.transactions().size() << '\n';
            for (const auto& tx : block.transactions())
                out << std::quoted(tx.sender()) << ' ' << std::quoted(tx.recipient()) << ' ' << tx.amount() << ' ' << tx.nonce() << '\n';
        }
        out.close();
        if (!out) throw std::runtime_error("Failed writing chain file");
    }
    // Preserve the old file if replacing it fails (including on Windows).
    auto backup = path_; backup += ".bak";
    if (std::filesystem::exists(backup)) throw std::runtime_error("Chain backup already exists; resolve it before saving");
    const bool had_file = exists();
    if (had_file) std::filesystem::rename(path_, backup);
    try { std::filesystem::rename(temp, path_); }
    catch (...) { if (had_file) std::filesystem::rename(backup, path_); throw; }
    if (had_file) std::filesystem::remove(backup);
}
}
