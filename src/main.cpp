#include "corecoin/module.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace corecoin::__cli {
void print_chain(const Blockchain& chain) {
    for (const auto& block : chain.blocks()) {
        std::cout << "Block " << block.height() << " | nonce " << block.nonce()
                  << " | transactions " << block.transactions().size() << '\n'
                  << "  " << block.hash() << '\n';
    }
}
void demo() {
    Blockchain chain;
    std::cout << "CoreCoin: local C++23 proof-of-work demo\n";
    chain.mine_pending("alice");
    chain.submit(Transaction("alice", "bob", 12 * gil_per_coin, chain.next_nonce("alice")));
    chain.mine_pending("miner");
    print_chain(chain);
    for (const auto* name : {"alice", "bob", "miner"})
        std::cout << name << ": " << format_amount(chain.balance(name)) << '\n';
    std::string error;
    if (!chain.validate(&error)) throw std::runtime_error(error);
    std::cout << "Chain validation: OK\n";
}
void shell() {
    Blockchain chain;
    std::cout << "CoreCoin local shell (in-memory, unsigned accounts)\n"
                 "mine ACCOUNT | send FROM TO GIL | balance ACCOUNT | chain | validate | quit\n"
                 "100000000 Gil = 1 CORE. Mine first to fund an account.\n";
    for (std::string line; std::cout << "> " && std::getline(std::cin, line);) {
        try {
            std::istringstream input(line);
            std::string command, a, b, extra;
            input >> command;
            if (command.empty()) continue;
            if (command == "quit") break;
            if (command == "mine" || command == "balance") {
                if (!(input >> a) || (input >> extra)) throw std::invalid_argument("Expected one account");
                if (command == "mine") std::cout << "Mined " << chain.mine_pending(a).hash() << '\n';
                else std::cout << format_amount(chain.balance(a)) << '\n';
            } else if (command == "send") {
                Amount amount{};
                if (!(input >> a >> b >> amount) || (input >> extra))
                    throw std::invalid_argument("Expected FROM TO integer Gil");
                Transaction tx(a, b, amount, chain.next_nonce(a));
                chain.submit(tx);
                std::cout << "Queued " << tx.id() << " (mine to confirm)\n";
            } else if (command == "chain") print_chain(chain);
            else if (command == "validate") {
                std::string error;
                std::cout << (chain.validate(&error) ? "Valid" : error) << '\n';
            } else throw std::invalid_argument("Unknown command");
        } catch (const std::exception& ex) { std::cout << "Error: " << ex.what() << '\n'; }
    }
}
}
int main(int argc, char** argv) {
    try {
        const std::string mode = argc > 1 ? argv[1] : "--demo";
        if (argc > 2 || (mode != "--demo" && mode != "--shell" && mode != "--help")) {
            std::cerr << "Usage: corecoin-cli [--demo|--shell|--help]\n";
            return 1;
        }
        if (mode == "--shell") corecoin::__cli::shell();
        else if (mode == "--demo") corecoin::__cli::demo();
        else std::cout << "Usage: corecoin-cli [--demo|--shell|--help]\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
