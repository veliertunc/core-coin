#include "module.hpp"
#include <utility>
namespace corecoin::__tests {
TEST(ValidationTests, CoreBehavior) {
    auto require = [&](bool value, const char* message) { EXPECT_TRUE(value) << message; };
    TransferChainFixture fixture;
    auto& chain = fixture.chain;
    auto forged = chain.blocks();
    auto replace_last = [&](std::vector<Transaction> txs, unsigned difficulty = 1,
                            std::string previous = "") {
        forged = chain.blocks();
        Block replacement(2, previous.empty() ? forged[1].hash() : previous,
                          forged[2].timestamp(), difficulty, std::move(txs));
        replacement.mine();
        forged[2] = std::move(replacement);
        return Blockchain::validate_chain(forged, {1, 50});
    };
    require(!replace_last({Transaction("", "miner", 51, 2)}), "Reject inflated reward with valid PoW");
    require(!replace_last({Transaction("", "miner", 50, 1)}), "Reject reward nonce");
    require(!replace_last({Transaction("alice", "bob", 51, 0), Transaction("", "miner", 50, 2)}), "Reject overspend in block");
    require(!replace_last({Transaction("", "miner", 50, 2), Transaction("", "miner", 50, 2)}), "Reject extra coinbase");
    require(!replace_last({}), "Reject missing reward");
    require(!replace_last({Transaction("", "miner", 50, 2)}, 0), "Reject easier difficulty");
    require(!replace_last({Transaction("", "miner", 50, 2)}, 1, std::string(64, 'f')), "Reject broken link");
    forged = chain.blocks();
    forged.erase(forged.begin());
    require(!Blockchain::validate_chain(forged, {1, 50}), "Reject missing genesis");
    require(!Blockchain::validate_chain({}, {1, 50}), "Reject empty chain");
}
}
