#pragma once
#include "blockchain.hpp"
#include <filesystem>
#include <iosfwd>

namespace corecoin {
// Versioned chain serialization. Reading validates the complete history.
class ChainCodec {
public:
    [[nodiscard]] static Blockchain read(std::istream& in);
    static void write(std::ostream& out, const Blockchain& chain);
};
// Stores confirmed blocks. Pending transactions are not persisted.
class ChainStore {
public:
    explicit ChainStore(std::filesystem::path path);
    [[nodiscard]] Blockchain load() const;
    void save(const Blockchain& chain) const;
    [[nodiscard]] bool exists() const;
    // Recover an interrupted replacement only when the primary file is absent.
    void recover() const;
private:
    std::filesystem::path path_;
};
}
