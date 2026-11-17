#include "corecoin/amount.hpp"

namespace corecoin {
std::string format_amount(Amount amount) {
    // Unsigned arithmetic also handles the minimum signed 64-bit value.
    const auto magnitude = amount < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(amount)
                                      : static_cast<std::uint64_t>(amount);
    auto fraction = std::to_string(magnitude % gil_per_coin);
    return (amount < 0 ? "-" : "") + std::to_string(magnitude / gil_per_coin) + "." +
           std::string(8 - fraction.size(), '0') + fraction + " CORE";
}
}
