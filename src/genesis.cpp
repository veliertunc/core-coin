#include "genesis.hpp"
#include "corecoin/network.hpp"

namespace corecoin::__detail {
Block genesis() {
    // Fixed allocation, committed by the genesis hash and checked on replay.
    return Block(0, std::string(64, '0'), 0, 0,
                 {Transaction("", std::string(initial_wallet_address), initial_wallet_allocation, 0)});
}
}
