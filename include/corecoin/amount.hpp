#pragma once
#include <cstdint>
#include <string>

namespace corecoin {
using Amount = std::int64_t; // Integer Gil.
inline constexpr Amount gil_per_coin = 100'000'000;
inline constexpr Amount units_per_coin = gil_per_coin;
[[nodiscard]] std::string format_amount(Amount amount);
}
