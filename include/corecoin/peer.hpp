#pragma once
#include "blockchain.hpp"
#include <cstdint>
#include <string>

namespace corecoin {
// One-shot TCP chain exchange. Peers are not authenticated or encrypted.
class Peer {
public:
    static void serve_once(const std::string& bind_address, std::uint16_t port, const Blockchain& chain);
    [[nodiscard]] static Blockchain fetch(const std::string& host, std::uint16_t port);
};
}
