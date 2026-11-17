#include "corecoin/amount.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>

namespace corecoin {
Amount parse_core_amount(std::string_view value) {
    const auto dot = value.find('.');
    const auto whole = value.substr(0, dot);
    const auto fraction = dot == std::string_view::npos ? std::string_view{} : value.substr(dot + 1);
    if (whole.empty() || (dot != std::string_view::npos && fraction.empty()) || fraction.size() > 8)
        throw std::invalid_argument("CORE amount requires up to 8 decimal places");
    std::uint64_t units = 0, decimal = 0;
    const auto parse = [](std::string_view part, std::uint64_t& result) {
        if (part.empty()) return true;
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), result);
        return error == std::errc{} && end == part.data() + part.size();
    };
    if (!parse(whole, units) || !parse(fraction, decimal)) throw std::invalid_argument("Invalid CORE amount");
    for (auto i = fraction.size(); i < 8; ++i) decimal *= 10;
    constexpr auto limit = static_cast<std::uint64_t>(std::numeric_limits<Amount>::max());
    if (units > (limit - decimal) / gil_per_coin) throw std::out_of_range("CORE amount is too large");
    return static_cast<Amount>(units * gil_per_coin + decimal);
}
std::string format_amount(Amount amount) {
    // Unsigned arithmetic also handles the minimum signed 64-bit value.
    const auto magnitude = amount < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(amount)
                                      : static_cast<std::uint64_t>(amount);
    auto fraction = std::to_string(magnitude % gil_per_coin);
    return (amount < 0 ? "-" : "") + std::to_string(magnitude / gil_per_coin) + "." +
           std::string(8 - fraction.size(), '0') + fraction + " CORE";
}
}
