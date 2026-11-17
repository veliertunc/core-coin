#pragma once
#include "corecoin/module.hpp"

namespace corecoin::__tests {
// A two-block chain with ordered transfers for replay and forged-block tests.
class TransferChainFixture {
public:
    TransferChainFixture();
    Blockchain chain{{1, 50}};
};

class WalletFixture {
public:
    Blockchain chain{{1, 50 * gil_per_coin}};
    Wallet founder{std::string(initial_wallet_address)};
    Wallet recipient{"recipient"};
};
}
