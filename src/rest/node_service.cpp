#include "rest/node_service.hpp"
#include "corecoin/consensus.hpp"
#include "corecoin/peer.hpp"
#include <utility>

namespace corecoin::__rest {
namespace {
Blockchain open_chain(const ChainStore& store) {
    store.recover();
    if (store.exists()) return store.load();
    Blockchain chain;
    store.save(chain);
    return chain;
}
}

NodeService::NodeService(std::filesystem::path chain_file)
    : store_(std::move(chain_file)), chain_(open_chain(store_)) {}

Transaction NodeService::build_locked(const TransferRequest& request) const {
    const auto nonce = request.nonce.value_or(chain_.next_nonce(request.sender));
    return Transaction(request.sender, request.recipient, request.amount, nonce, request.public_key, request.signature);
}
Transaction NodeService::build_transfer(const TransferRequest& request) const {
    std::scoped_lock lock(mutex_);
    return build_locked(request);
}
Transaction NodeService::submit(const TransferRequest& request) {
    std::scoped_lock lock(mutex_);
    auto transaction = build_locked(request);
    chain_.submit(transaction);  // Leaves the chain unchanged when rejected.
    return transaction;
}
Block NodeService::mine(const std::string& miner) {
    std::scoped_lock lock(mutex_);
    auto candidate = chain_;
    auto block = candidate.mine_pending(miner);
    store_.save(candidate);
    chain_ = std::move(candidate);
    return block;
}
SyncResult NodeService::sync(const std::string& host, std::uint16_t port) {
    auto candidate = Peer::fetch(host, port);  // Network I/O happens without holding the lock.
    std::scoped_lock lock(mutex_);
    if (!Consensus::prefer_candidate(chain_, candidate))
        return {false, chain_.blocks().back().height()};
    store_.save(candidate);
    chain_ = std::move(candidate);
    return {true, chain_.blocks().back().height()};
}
}
