#pragma once
#include "blockchain.hpp"
#include <filesystem>

namespace corecoin {
// Stores confirmed blocks. Pending transactions are not persisted.
class ChainStore {
public:
    explicit ChainStore(std::filesystem::path path);
    [[nodiscard]] Blockchain load() const;
    void save(const Blockchain& chain) const;
    [[nodiscard]] bool exists() const;
private:
    std::filesystem::path path_;
};
}
