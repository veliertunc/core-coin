#include "corecoin/module.hpp"
#include "module.hpp"
#include <stdexcept>

namespace corecoin::__tests {
TEST(WalletTests, CoreBehavior) {
    auto check = [&](bool value, const char* message) { EXPECT_TRUE(value) << message; };
    WalletFixture fixture;
    auto& chain = fixture.chain;
    auto& founder = fixture.founder;
    auto& recipient = fixture.recipient;
    check(founder.balance(chain) == 11'000 * gil_per_coin, "Initial wallet must have exactly 11000 CORE");
    check(chain.blocks().front().transactions().size() == 1, "Genesis commits to allocation");
    check(chain.validate(), "Funded genesis validates");
    check(recipient.balance(chain) == 0, "New wallet must not create funds");
    founder.send(chain, recipient.address(), 10 * gil_per_coin);
    check(founder.balance(chain) == initial_wallet_allocation, "Pending send is unconfirmed");
    check(founder.available_balance(chain) == initial_wallet_allocation - 10 * gil_per_coin,
          "Pending transfer reserves wallet funds");
    bool rejected = false;
    try { founder.send(chain, recipient.address(), initial_wallet_allocation); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Wallet cannot spend reserved funds twice");
    chain.mine_pending("miner");
    check(recipient.balance(chain) == 10 * gil_per_coin, "Recipient receives after mining");
    recipient.send(chain, founder.address(), gil_per_coin);
    chain.mine_pending("miner");
    check(founder.balance(chain) == 10'991 * gil_per_coin, "Founder receives return transfer");
    check(recipient.balance(chain) == 9 * gil_per_coin, "Recipient can send coins");
    check(chain.validate(), "Wallet round trip independently replays");
    check(founder.balance(chain) + recipient.balance(chain) + chain.balance("miner") ==
          initial_wallet_allocation + 100 * gil_per_coin, "Supply equals allocation plus rewards");
    const Wallet reopened{std::string(initial_wallet_address)};
    check(reopened.balance(chain) == founder.balance(chain), "Opening wallet cannot mint another allocation");
    auto forged = chain.blocks();
    forged[0] = Block(0, std::string(64, '0'), 0, 0,
                      {Transaction("", "founder", initial_wallet_allocation + 1, 0)});
    check(!Blockchain::validate_chain(forged, {1, 50 * gil_per_coin}), "Reject altered genesis allocation");
    rejected = false;
    try { Wallet invalid("invalid address"); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Reject invalid wallet address");
}
}
