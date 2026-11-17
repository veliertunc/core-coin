#include "corecoin/module.hpp"
#include "cli_password.hpp"
#include <openssl/crypto.h>
#include <iostream>
#include <iomanip>
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
void shell(const std::string& path) {
    ChainStore store(path);
    store.recover();
    Blockchain chain = store.exists() ? store.load() : Blockchain{};
    if (!store.exists()) store.save(chain);
    Wallet active{std::string(initial_wallet_address)};
    std::cout << "CoreCoin local shell (persistent chain; named accounts are unsigned)\n"
                 "wallet ADDRESS | wallet-create FILE | wallet-open FILE | receive | send TO GIL | balance [ADDRESS]\n"
                 "send-core TO CORE | history [ADDRESS] | mine ACCOUNT | chain | validate | quit\n"
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
            if (command == "wallet-create" || command == "wallet-open") {
                if (!(input >> std::quoted(a)) || (input >> extra)) throw std::invalid_argument("Expected wallet file path");
                auto password = read_password("Wallet password: ");
                struct PasswordCleanup { std::string& value; ~PasswordCleanup() { OPENSSL_cleanse(value.data(), value.size()); } } cleanup{password};
                if (command == "wallet-create") {
                    auto confirmation = read_password("Confirm password: ");
                    PasswordCleanup confirmation_cleanup{confirmation};
                    if (password != confirmation) throw std::invalid_argument("Passwords do not match");
                    auto key = SigningKey::generate();
                    key.save(a, password);
                    active = Wallet(std::move(key));
                    std::cout << "Created encrypted wallet. Back up its file and password separately.\n";
                } else active = Wallet(SigningKey::load(a, password));
                std::cout << "Selected wallet: " << active.address() << " | " << format_amount(active.balance(chain)) << '\n';
            } else if (command == "wallet") {
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
                auto candidate = chain;
                const auto hash = candidate.mine_pending(a).hash();
                store.save(candidate);
                chain = std::move(candidate);
                std::cout << "Mined " << hash << '\n';
            } else if (command == "history") {
                if (!(input >> a)) a = active.address();
                if (input >> extra) throw std::invalid_argument("Expected at most one address");
                const Wallet wallet(a);
                for (const auto& block : chain.blocks()) {
                    for (const auto& tx : block.transactions()) {
                        if (tx.sender() == a || tx.recipient() == a)
                            std::cout << "Confirmed block " << block.height() << " " << tx.id() << " "
                                      << (tx.sender().empty() ? "issuance" : tx.sender()) << " -> "
                                      << tx.recipient() << " " << format_amount(tx.amount()) << '\n';
                    }
                }
                for (const auto& tx : chain.pending()) {
                    if (tx.sender() == a || tx.recipient() == a)
                        std::cout << "Pending " << tx.id() << " " << tx.sender() << " -> "
                                  << tx.recipient() << " " << format_amount(tx.amount()) << '\n';
                }
            } else if (command == "send-core") {
                if (!(input >> a >> b) || (input >> extra)) throw std::invalid_argument("Expected TO CORE");
                const auto id = active.send(chain, a, parse_core_amount(b));
                std::cout << "Queued " << id << " (mine to confirm)\n";
            } else if (command == "send") {
                if (!(input >> a >> b)) throw std::invalid_argument("Expected TO integer Gil");
                std::string sender = active.address(), recipient = a, value = b;
                bool explicit_sender = false;
                if (input >> extra) { sender = a; recipient = b; value = extra; explicit_sender = true; }
                if (input >> extra) throw std::invalid_argument("Too many send arguments");
                Amount amount{};
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), amount);
                if (error != std::errc{} || end != value.data() + value.size())
                    throw std::invalid_argument("Amount must be integer Gil");
                const auto id = explicit_sender ? Wallet(sender).send(chain, recipient, amount) : active.send(chain, recipient, amount);
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
        if (argc > 3 || (argc == 3 && mode != "--shell") || (mode != "--demo" && mode != "--shell" && mode != "--help")) {
            std::cerr << "Usage: corecoin-cli [--demo|--shell [CHAIN_FILE]|--help]\n";
            return 1;
        }
        if (mode == "--shell") corecoin::__cli::shell(argc == 3 ? argv[2] : "corecoin.chain");
        else if (mode == "--demo") corecoin::__cli::demo();
        else std::cout << "Usage: corecoin-cli [--demo|--shell [CHAIN_FILE]|--help]\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
