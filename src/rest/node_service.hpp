#pragma once
#include "corecoin/blockchain.hpp"
#include "corecoin/chain_store.hpp"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace corecoin::__rest {
// Fields of a transfer submitted by an API client. A missing nonce uses the
// sender's next nonce (only meaningful for unsigned legacy accounts).
struct TransferRequest {
    std::string sender, recipient;
    Amount amount = 0;
    std::optional<std::uint64_t> nonce;
    std::string public_key, signature;
};

struct SyncResult {
    bool adopted = false;
    std::uint64_t height = 0;
};

// Owns one persistent chain for the API. All access is serialized; confirmed
// blocks are saved before they become visible, pending transfers stay in memory.
class NodeService {
public:
    explicit NodeService(std::filesystem::path chain_file);

    template <class F>
    decltype(auto) read(F&& reader) const {
        std::scoped_lock lock(mutex_);
        return std::forward<F>(reader)(std::as_const(chain_));
    }
    [[nodiscard]] Transaction build_transfer(const TransferRequest& request) const;
    Transaction submit(const TransferRequest& request);
    Block mine(const std::string& miner);
    SyncResult sync(const std::string& host, std::uint16_t port);

private:
    [[nodiscard]] Transaction build_locked(const TransferRequest& request) const;
    ChainStore store_;
    Blockchain chain_;
    mutable std::mutex mutex_;
};
}
