#include "rest/presenter.hpp"
#include "corecoin/network.hpp"

namespace corecoin::__rest {
namespace {
constexpr std::string_view network_name = "corecoin-local";
constexpr std::string_view unit_suffix = " CORE";
}

std::string Presenter::core_string(Amount amount) {
    auto text = format_amount(amount);
    if (text.ends_with(unit_suffix)) text.resize(text.size() - unit_suffix.size());
    return text;
}

Json Presenter::transaction(const Transaction& tx) {
    auto json = Json::object();
    json.set("id", tx.id())
        .set("type", tx.sender().empty() ? "issuance" : "transfer")
        .set("sender", tx.sender().empty() ? Json(nullptr) : Json(tx.sender()))
        .set("recipient", tx.recipient())
        .set("amount", tx.amount())
        .set("amount_core", core_string(tx.amount()))
        .set("nonce", tx.nonce())
        .set("public_key", tx.public_key().empty() ? Json(nullptr) : Json(tx.public_key()))
        .set("signature", tx.signature().empty() ? Json(nullptr) : Json(tx.signature()));
    return json;
}

Json Presenter::block_summary(const Block& block) {
    auto json = Json::object();
    json.set("height", block.height())
        .set("hash", block.hash())
        .set("previous_hash", block.previous_hash())
        .set("timestamp", block.timestamp())
        .set("difficulty", block.difficulty())
        .set("nonce", block.nonce())
        .set("transaction_count", block.transactions().size());
    return json;
}

Json Presenter::block(const Block& block) {
    auto json = block_summary(block);
    auto transactions = Json::array();
    for (const auto& tx : block.transactions()) transactions.push(transaction(tx));
    json.set("transactions", std::move(transactions));
    return json;
}

Json Presenter::account(const Blockchain& chain, const std::string& address) {
    const auto balance = chain.balance(address);
    const auto available = chain.available_balance(address);
    auto json = Json::object();
    json.set("address", address)
        .set("key_backed", address.size() == 67 && address.starts_with("cc_"))
        .set("balance", balance)
        .set("balance_core", core_string(balance))
        .set("available_balance", available)
        .set("available_balance_core", core_string(available))
        .set("next_nonce", chain.next_nonce(address));
    return json;
}

Json Presenter::node(const Blockchain& chain) {
    Amount issued = 0;
    std::size_t transfers = 0;
    for (const auto& block : chain.blocks())
        for (const auto& tx : block.transactions()) {
            if (tx.sender().empty()) issued += tx.amount();
            else ++transfers;
        }
    const auto& tip = chain.blocks().back();
    const auto config = chain.config();
    auto json = Json::object();
    json.set("network", network_name)
        .set("height", tip.height())
        .set("tip_hash", tip.hash())
        .set("genesis_hash", chain.blocks().front().hash())
        .set("difficulty", config.difficulty)
        .set("mining_reward", config.mining_reward)
        .set("mining_reward_core", core_string(config.mining_reward))
        .set("issued_supply", issued)
        .set("issued_supply_core", core_string(issued))
        .set("maximum_supply", maximum_supply)
        .set("maximum_supply_core", core_string(maximum_supply))
        .set("gil_per_core", gil_per_coin)
        .set("confirmed_transfers", transfers)
        .set("pending_transactions", chain.pending().size());
    return json;
}

Json Presenter::located_transaction(const Transaction& tx, std::optional<std::uint64_t> block_height) {
    auto json = Json::object();
    json.set("status", block_height ? "confirmed" : "pending")
        .set("block_height", block_height ? Json(*block_height) : Json(nullptr))
        .set("transaction", transaction(tx));
    return json;
}
}
