#include "corecoin/module.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <charconv>

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
    Wallet founder{std::string(initial_wallet_address)};
    Wallet bob("bob");
    std::cout << "CoreCoin: founder wallet starts with " << format_amount(founder.balance(chain)) << '\n';
    founder.send(chain, bob.address(), 12 * gil_per_coin);
    chain.mine_pending("miner");
    bob.send(chain, founder.address(), 2 * gil_per_coin);
    chain.mine_pending("miner");
    print_chain(chain);
    for (const auto* name : {"founder", "bob", "miner"})
        std::cout << name << ": " << format_amount(chain.balance(name)) << '\n';
    std::string error;
    if (!chain.validate(&error)) throw std::runtime_error(error);
    std::cout << "Chain validation: OK\n";
}
void shell() {
    Blockchain chain;
    Wallet active{std::string(initial_wallet_address)};
    std::cout << "CoreCoin local shell (in-memory, unsigned accounts)\n"
                 "wallet ADDRESS | receive | send TO GIL | balance [ADDRESS]\n"
                 "mine ACCOUNT | chain | validate | quit\n"
                 "Legacy send FROM TO GIL is also supported.\n"
                 "100000000 Gil = 1 CORE. Mine after sending to confirm transfers.\n"
              << "Selected wallet: " << active.address() << " | " << format_amount(active.balance(chain)) << '\n';
    for (std::string line; std::cout << "> " && std::getline(std::cin, line);) {
        try {
            std::istringstream input(line);
            std::string command, a, b, extra;
            input >> command;
            if (command.empty()) continue;
            if (command == "quit") break;
            if (command == "wallet") {
                if (!(input >> a) || (input >> extra)) throw std::invalid_argument("Expected wallet address");
                active = Wallet(a);
                std::cout << "Selected wallet: " << active.address() << " | " << format_amount(active.balance(chain)) << '\n';
            } else if (command == "receive" || command == "address") {
                if (input >> extra) throw std::invalid_argument("Receive takes no arguments");
                std::cout << "Receive address: " << active.address() << " (share with sender; mine to confirm)\n";
            } else if (command == "balance") {
                if (!(input >> a)) a = active.address();
                if (input >> extra) throw std::invalid_argument("Expected at most one address");
                const Wallet wallet(a);
                std::cout << "Confirmed: " << format_amount(wallet.balance(chain))
                          << " | Including pending: " << format_amount(wallet.available_balance(chain)) << '\n';
            } else if (command == "mine") {
                if (!(input >> a) || (input >> extra)) throw std::invalid_argument("Expected one account");
                std::cout << "Mined " << chain.mine_pending(a).hash() << '\n';
            } else if (command == "send") {
                if (!(input >> a >> b)) throw std::invalid_argument("Expected TO integer Gil");
                std::string sender = active.address(), recipient = a, value = b;
                if (input >> extra) { sender = a; recipient = b; value = extra; }
                if (input >> extra) throw std::invalid_argument("Too many send arguments");
                Amount amount{};
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), amount);
                if (error != std::errc{} || end != value.data() + value.size())
                    throw std::invalid_argument("Amount must be integer Gil");
                const auto id = Wallet(sender).send(chain, recipient, amount);
                std::cout << "Queued " << id << " (mine to confirm)\n";
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
