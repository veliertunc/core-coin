#include "module.hpp"
#include "../src/module.hpp"
#include <filesystem>
namespace corecoin::__tests {
TEST(CryptoTests, CoreBehavior) {
    auto require = [&](bool value, const char* message) { EXPECT_TRUE(value) << message; };
    using corecoin::__detail::sha256;
    require(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA empty vector");
    require(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA abc vector");
    require(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "SHA padding vector");
    require(sha256(std::string(1'000'000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "SHA multi-block vector");
    require(Transaction("ab", "c", 1, 0).id() != Transaction("a", "bc", 1, 0).id(), "Unambiguous encoding");
    const auto key = SigningKey::generate();
    const auto other = SigningKey::generate();
    const auto signature = key.sign("transfer");
    require(key.address().size() == 67 && key.address() != other.address(), "Unique key-derived addresses");
    require(SigningKey::verify(key.public_key(), "transfer", signature), "Valid Ed25519 signature");
    require(!SigningKey::verify(key.public_key(), "changed", signature), "Reject modified signed message");
    require(!SigningKey::verify(other.public_key(), "transfer", signature), "Reject wrong public key");
    require(!SigningKey::verify(key.public_key(), "transfer", "bad"), "Reject malformed signature");
    const auto directory = std::filesystem::temp_directory_path() / ("corecoin-test-" + key.address());
    if (!std::filesystem::create_directory(directory)) throw std::runtime_error("Cannot create wallet test directory");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); } } cleanup{directory};
    const auto path = directory / "wallet.pem";
    key.save(path, "correct horse battery staple");
    const auto loaded = SigningKey::load(path, "correct horse battery staple");
    require(loaded.public_key() == key.public_key(), "Encrypted key survives reload");
    require(SigningKey::verify(loaded.public_key(), "transfer", loaded.sign("transfer")), "Reloaded key can sign");
    bool wrong_password = false;
    try { (void)SigningKey::load(path, "incorrect horse battery staple"); } catch (const std::exception&) { wrong_password = true; }
    require(wrong_password, "Reject wrong wallet password");
    bool overwrite = false;
    try { other.save(path, "correct horse battery staple"); } catch (const std::exception&) { overwrite = true; }
    require(overwrite, "Reject wallet key overwrite");
    Blockchain chain({1, 50});
    Wallet founder{std::string(initial_wallet_address)};
    Wallet secure(key);
    founder.send(chain, secure.address(), 100);
    chain.mine_pending("miner");
    bool unsigned_rejected = false;
    try { chain.submit(Transaction(secure.address(), "bob", 1, 0)); } catch (const std::exception&) { unsigned_rejected = true; }
    require(unsigned_rejected, "Reject unsigned key-backed spend");
    secure.send(chain, "bob", 40);
    const auto signed_transaction = chain.pending().front();
    bool tampered_rejected = false;
    try { chain.submit(Transaction(secure.address(), "bob", 41, 0, signed_transaction.public_key(), signed_transaction.signature())); }
    catch (const std::exception&) { tampered_rejected = true; }
    require(tampered_rejected, "Reject altered signed amount");
    chain.mine_pending("miner");
    require(chain.balance("bob") == 40 && chain.validate(), "Signed transfer confirms and replays");
    const ChainStore store(directory / "chain.dat");
    store.save(chain);
    auto restored = store.load();
    require(restored.validate() && restored.balance("bob") == 40, "Signed transaction survives storage");
    Wallet reopened(loaded);
    reopened.send(restored, "bob", 10);
    restored.mine_pending("miner");
    require(restored.balance("bob") == 50 && restored.validate(), "Reloaded key can spend after chain reload");
    auto forged = chain.blocks();
    const auto& original = forged.back();
    auto altered = original.transactions();
    altered[0] = Transaction(secure.address(), "bob", 41, 0, signed_transaction.public_key(), signed_transaction.signature());
    Block forged_block(original.height(), original.previous_hash(), original.timestamp(), original.difficulty(), std::move(altered));
    forged_block.mine();
    forged.back() = std::move(forged_block);
    require(!Blockchain::validate_chain(forged, {1, 50}), "Reject forged signed transaction during replay");
}
}
