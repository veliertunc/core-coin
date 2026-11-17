#include "corecoin/consensus.hpp"
#include <stdexcept>

namespace corecoin {
bool Consensus::prefer_candidate(const Blockchain& current, const Blockchain& candidate) {
    if (!current.validate() || !candidate.validate()) throw std::invalid_argument("Invalid chain");
    const auto ours = current.config(), theirs = candidate.config();
    if (ours.difficulty != theirs.difficulty || ours.mining_reward != theirs.mining_reward ||
        current.blocks().front().hash() != candidate.blocks().front().hash())
        throw std::invalid_argument("Peer belongs to an incompatible network");
    if (candidate.blocks().size() <= current.blocks().size()) return false;
    if (!current.pending().empty()) throw std::invalid_argument("Cannot replace chain while transactions are pending");
    return true;
}
}
