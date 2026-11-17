#include "module.hpp"
#include <stdexcept>

namespace corecoin::__tests {
TEST(ConsensusTests, PrefersOnlyLongerValidCompatibleChain) {
    Blockchain local({1, 50});
    auto remote = local;
    EXPECT_FALSE(Consensus::prefer_candidate(local, remote));
    remote.mine_pending("miner");
    EXPECT_TRUE(Consensus::prefer_candidate(local, remote));
    EXPECT_FALSE(Consensus::prefer_candidate(remote, local));
    auto fork = local;
    fork.mine_pending("another-miner");
    EXPECT_FALSE(Consensus::prefer_candidate(fork, remote));
}
TEST(ConsensusTests, RejectsOtherNetworkParameters) {
    Blockchain local({1, 50});
    Blockchain other_difficulty({2, 50});
    Blockchain other_reward({1, 51});
    EXPECT_THROW((void)Consensus::prefer_candidate(local, other_difficulty), std::invalid_argument);
    EXPECT_THROW((void)Consensus::prefer_candidate(local, other_reward), std::invalid_argument);
}
TEST(ConsensusTests, PendingTransactionsCannotBeDiscardedByReorganization) {
    Blockchain local({1, 50});
    auto remote = local;
    remote.mine_pending("miner");
    local.submit(Transaction("founder", "bob", 1, 0));
    EXPECT_THROW((void)Consensus::prefer_candidate(local, remote), std::invalid_argument);
    EXPECT_EQ(local.pending().size(), 1u);
    EXPECT_EQ(local.available_balance("bob"), 1);
    EXPECT_FALSE(Consensus::prefer_candidate(local, Blockchain({1, 50})));
}
}
