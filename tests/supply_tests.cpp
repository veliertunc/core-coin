#include "module.hpp"
#include <limits>
#include <utility>
namespace corecoin::__tests {
void supply_tests(TestContext& context) {
    auto require = [&](bool value, const char* message) { context.require(value, message); };
    Blockchain large({1, std::numeric_limits<Amount>::max()});
    large.mine_pending("alice");
    require(large.balance("alice") == maximum_supply - initial_wallet_allocation, "Reward capped at remaining supply");
    large.mine_pending("alice");
    require(large.blocks().back().transactions().back().amount() == 0 && large.validate(), "Mining continues without rewards at cap");
    large.submit(Transaction("alice", "bob", 1, 0));
    large.mine_pending("miner");
    require(large.balance("bob") == 1 && large.validate(), "Transfers remain valid after supply cap");
    auto over_cap = large.blocks();
    Block inflated(2, over_cap[1].hash(), over_cap[2].timestamp(), 1, {Transaction("", "alice", 1, 2)});
    inflated.mine();
    over_cap[2] = std::move(inflated);
    require(!Blockchain::validate_chain(over_cap, {1, std::numeric_limits<Amount>::max()}), "Reject issuance above cap");
}
}
