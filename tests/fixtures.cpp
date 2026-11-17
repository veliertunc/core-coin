#include "fixtures.hpp"

namespace corecoin::__tests {
TransferChainFixture::TransferChainFixture() {
    chain.mine_pending("alice");
    chain.submit(Transaction("alice", "bob", 30, 0));
    chain.submit(Transaction("alice", "bob", 20, 1));
    chain.submit(Transaction("bob", "carol", 5, 0));
    chain.mine_pending("miner");
}
}
