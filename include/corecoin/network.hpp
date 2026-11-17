#pragma once
#include "amount.hpp"
#include <string_view>

namespace corecoin {
inline constexpr std::string_view initial_wallet_address = "founder";
inline constexpr Amount initial_wallet_allocation = 11'000 * gil_per_coin;
inline constexpr Amount maximum_supply = 11'000'000 * gil_per_coin;
}
