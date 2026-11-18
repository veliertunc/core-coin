#pragma once
#include "corecoin/blockchain.hpp"
#include "rest/json.hpp"
#include <optional>
#include <string>

namespace corecoin::__rest {
// Converts domain objects into the API's JSON representations.
class Presenter {
public:
    [[nodiscard]] static std::string core_string(Amount amount);
    [[nodiscard]] static Json transaction(const Transaction& tx);
    [[nodiscard]] static Json block_summary(const Block& block);
    [[nodiscard]] static Json block(const Block& block);
    [[nodiscard]] static Json account(const Blockchain& chain, const std::string& address);
    [[nodiscard]] static Json node(const Blockchain& chain);
    // A transaction with its confirmation status; block_height is absent when pending.
    [[nodiscard]] static Json located_transaction(const Transaction& tx, std::optional<std::uint64_t> block_height);
};
}
