#include "corecoin/module.hpp"
#include "../src/module.hpp"
#include "module.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace corecoin::__tests {
using corecoin::__detail::sha256;
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    require(rejected, message);
}
void tests() {
    require(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA empty vector");
    require(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA abc vector");
    require(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "SHA padding vector");
    require(sha256(std::string(1'000'000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "SHA multi-block vector");
    require(Transaction("ab", "c", 1, 0).id() != Transaction("a", "bc", 1, 0).id(), "Unambiguous encoding");
    rejects([] { Blockchain bad({0, 50}); }, "Reject zero difficulty");
    rejects([] { Blockchain bad({1, 0}); }, "Reject zero reward");
    Blockchain chain({1, 50});
    require(chain.validate(), "Genesis validates");
    require(chain.balance("alice") == 0, "Unfunded account starts empty");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 0)); }, "Reject unfunded spend");
    chain.mine_pending("alice");
    require(chain.balance("alice") == 50, "Mining reward");
    rejects([&] { chain.submit(Transaction("alice", "bob", 0, 0)); }, "Reject zero");
    rejects([&] { chain.submit(Transaction("alice", "bob", -1, 0)); }, "Reject negative");
    rejects([&] { chain.submit(Transaction("alice", "alice", 1, 0)); }, "Reject self transfer");
    rejects([&] { chain.submit(Transaction("alice", "bad account", 1, 0)); }, "Reject bad address");
    rejects([&] { chain.submit(Transaction("", "bob", 50, 0)); }, "Reject user coinbase");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 1)); }, "Reject future nonce");
    chain.submit(Transaction("alice", "bob", 30, 0));
    require(chain.balance("bob") == 0 && chain.next_nonce("alice") == 1, "Pending remains unconfirmed");
    rejects([&] { chain.submit(Transaction("alice", "bob", 30, 1)); }, "Reject pending double spend");
    rejects([&] { chain.submit(Transaction("alice", "bob", 1, 0)); }, "Reject replay");
    chain.submit(Transaction("alice", "bob", 20, 1));
    chain.submit(Transaction("bob", "carol", 5, 0));
    rejects([&] { chain.mine_pending("bad account"); }, "Reject bad miner");
    require(chain.pending().size() == 3 && chain.blocks().size() == 2, "Failed mining is atomic");
    chain.mine_pending("miner");
    require(chain.balance("alice") == 0 && chain.balance("bob") == 45 && chain.balance("carol") == 5 &&
            chain.balance("miner") == 50, "Transfers and rewards accounted");
    require(chain.pending().empty() && chain.validate(), "Mined chain validates");
    rejects([&] { chain.submit(Transaction("bob", "alice", 1, 0)); }, "Reject confirmed replay");
    auto forged = chain.blocks();
    auto replace_last = [&](std::vector<Transaction> txs, unsigned difficulty = 1,
                            std::string previous = "") {
        forged = chain.blocks();
        Block replacement(2, previous.empty() ? forged[1].hash() : previous,
                          forged[2].timestamp(), difficulty, std::move(txs));
        replacement.mine();
        forged[2] = std::move(replacement);
        return Blockchain::validate_chain(forged, {1, 50});
    };
    require(!replace_last({Transaction("", "miner", 51, 2)}), "Reject inflated reward with valid PoW");
    require(!replace_last({Transaction("", "miner", 50, 1)}), "Reject reward nonce");
    require(!replace_last({Transaction("alice", "bob", 51, 0), Transaction("", "miner", 50, 2)}), "Reject overspend in block");
    require(!replace_last({Transaction("", "miner", 50, 2), Transaction("", "miner", 50, 2)}), "Reject extra coinbase");
    require(!replace_last({}), "Reject missing reward");
    require(!replace_last({Transaction("", "miner", 50, 2)}, 0), "Reject easier difficulty");
    require(!replace_last({Transaction("", "miner", 50, 2)}, 1, std::string(64, 'f')), "Reject broken link");
    forged = chain.blocks();
    forged.erase(forged.begin());
    require(!Blockchain::validate_chain(forged, {1, 50}), "Reject missing genesis");
    require(!Blockchain::validate_chain({}, {1, 50}), "Reject empty chain");
    Blockchain large({1, std::numeric_limits<Amount>::max()});
    large.mine_pending("alice");
    rejects([&] { large.mine_pending("alice"); }, "Reject balance overflow");
    require(large.blocks().size() == 2 && large.validate(), "Overflow leaves state intact");
    require(format_amount(1) == "0.00000001 CORE", "One Gil formatting");
    require(format_amount(std::numeric_limits<Amount>::min()) == "-92233720368.54775808 CORE", "Signed minimum formatting");
}
}
int main() {
    try {
        corecoin::__tests::tests();
        corecoin::__tests::checks += corecoin::__tests::wallet_tests();
        std::cout << corecoin::__tests::checks << " checks passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Check " << corecoin::__tests::checks << " failed: " << ex.what() << '\n';
        return 1;
    }
}
