#include "module.hpp"

namespace corecoin::__tests {
TEST(BlockchainTests, CoreBehavior) {
    auto require = [&](bool value, const char* message) { EXPECT_TRUE(value) << message; };
    auto rejects = [&](auto action, const char* message) { EXPECT_ANY_THROW(action()) << message; };
    rejects([] { Blockchain bad({0, 50}); }, "Reject zero difficulty");
    rejects([] { Blockchain bad({1, 0}); }, "Reject zero reward");
    Blockchain chain({1, 50});
    require(chain.validate(), "Genesis validates");
    require(chain.balance("alice") == 0, "Unfunded account starts empty");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 0)); }, "Reject unfunded spend");
    chain.mine_pending("alice");
    require(chain.balance("alice") == 50, "Mining reward");
    rejects([&] { chain.submit(Transaction("alice", "bob", 0, 0)); }, "Reject zero");
    rejects([&] { chain.submit(Transaction("alice", "bob", -1, 0)); }, "Reject negative");
    rejects([&] { chain.submit(Transaction("alice", "alice", 1, 0)); }, "Reject self transfer");
    rejects([&] { chain.submit(Transaction("alice", "bad account", 1, 0)); }, "Reject bad address");
    rejects([&] { chain.submit(Transaction("", "bob", 50, 0)); }, "Reject user coinbase");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 1)); }, "Reject future nonce");
    chain.submit(Transaction("alice", "bob", 30, 0));
    require(chain.balance("bob") == 0 && chain.next_nonce("alice") == 1, "Pending remains unconfirmed");
    rejects([&] { chain.submit(Transaction("alice", "bob", 30, 1)); }, "Reject pending double spend");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 0)); }, "Reject replay");
    chain.submit(Transaction("alice", "bob", 20, 1));
    chain.submit(Transaction("bob", "carol", 5, 0));
    rejects([&] { chain.mine_pending("bad account"); }, "Reject bad miner");
    require(chain.pending().size() == 3 && chain.blocks().size() == 2, "Failed mining is atomic");
    chain.mine_pending("miner");
    require(chain.balance("alice") == 0 && chain.balance("bob") == 45 && chain.balance("carol") == 5 &&
            chain.balance("miner") == 50, "Transfers and rewards accounted");
    require(chain.pending().empty() && chain.validate(), "Mined chain validates");
    rejects([&] { chain.submit(Transaction("bob", "alice", 1, 0)); }, "Reject confirmed replay");
}
}
