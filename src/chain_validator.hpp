#pragma once
#include "corecoin/blockchain.hpp"

namespace corecoin::__detail {
void check_config(ChainConfig config);
class ChainValidator {
public:
    [[nodiscard]] static bool validate(const std::vector<Block>& blocks, ChainConfig config, std::string* error);
};
}
