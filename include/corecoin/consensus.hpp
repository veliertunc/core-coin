#pragma once
#include "blockchain.hpp"
namespace corecoin {
// Fixed-difficulty chain selection: higher height means more cumulative work.
class Consensus {
public:
    [[nodiscard]] static bool prefer_candidate(const Blockchain& current, const Blockchain& candidate);
};
}
