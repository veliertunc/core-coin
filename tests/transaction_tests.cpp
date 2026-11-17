#include "module.hpp"
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

namespace corecoin::__tests {
class SignedTransactionFixture : public ::testing::Test {
protected:
    SigningKey key = SigningKey::generate();
    SigningKey other = SigningKey::generate();
    Wallet wallet{key};
    Blockchain chain{{1, 50}};
    void SetUp() override {
        Wallet founder{std::string(initial_wallet_address)};
        founder.send(chain, wallet.address(), 100);
        chain.mine_pending("miner");
    }
    Transaction signed_transfer(std::string recipient, Amount amount, std::uint64_t nonce) const {
        const auto pub = key.public_key();
        Transaction unsigned_tx(wallet.address(), recipient, amount, nonce, pub);
        return Transaction(wallet.address(), std::move(recipient), amount, nonce, pub, key.sign(unsigned_tx.signing_payload()));
    }
};

TEST_F(SignedTransactionFixture, AcceptsValidSignatureAndPreservesPendingBalances) {
    const auto transaction = signed_transfer("bob", 40, 0);
    EXPECT_NO_THROW(chain.submit(transaction));
    EXPECT_EQ(chain.balance(wallet.address()), 100);
    EXPECT_EQ(chain.available_balance(wallet.address()), 60);
    EXPECT_EQ(chain.available_balance("bob"), 40);
    EXPECT_EQ(chain.next_nonce(wallet.address()), 1u);
    chain.mine_pending("miner");
    EXPECT_EQ(chain.balance("bob"), 40);
    EXPECT_TRUE(chain.validate());
}
TEST_F(SignedTransactionFixture, RejectsModifiedSignedFieldsWithoutChangingState) {
    const auto tx = signed_transfer("bob", 40, 0);
    const auto sig = tx.signature();
    const auto pub = tx.public_key();
    const Transaction variants[] = {
        {wallet.address(), "carol", 40, 0, pub, sig},
        {wallet.address(), "bob", 41, 0, pub, sig},
        {wallet.address(), "bob", 40, 1, pub, sig},
        {other.address(), "bob", 40, 0, pub, sig},
        {wallet.address(), "bob", 40, 0, other.public_key(), sig},
        {wallet.address(), "bob", 40, 0, pub, std::string(128, '0')},
        {wallet.address(), "bob", 40, 0, pub, "bad"},
        {wallet.address(), "bob", 40, 0},
    };
    for (const auto& variant : variants) {
        EXPECT_THROW(chain.submit(variant), std::exception);
        EXPECT_TRUE(chain.pending().empty());
        EXPECT_EQ(chain.available_balance(wallet.address()), 100);
        EXPECT_EQ(chain.next_nonce(wallet.address()), 0u);
    }
    EXPECT_NO_THROW(chain.submit(tx));
}
TEST_F(SignedTransactionFixture, RejectsReplaysBeforeAndAfterConfirmation) {
    const auto tx = signed_transfer("bob", 40, 0);
    chain.submit(tx);
    EXPECT_THROW(chain.submit(tx), std::invalid_argument);
    chain.mine_pending("miner");
    EXPECT_THROW(chain.submit(tx), std::invalid_argument);
    EXPECT_EQ(chain.balance("bob"), 40);
}
TEST_F(SignedTransactionFixture, RejectsOverspendingIncludingPendingTransfers) {
    chain.submit(signed_transfer("bob", 60, 0));
    EXPECT_THROW(chain.submit(signed_transfer("carol", 41, 1)), std::invalid_argument);
    EXPECT_EQ(chain.pending().size(), 1u);
    EXPECT_EQ(chain.available_balance(wallet.address()), 40);
    EXPECT_EQ(chain.next_nonce(wallet.address()), 1u);
    EXPECT_NO_THROW(chain.submit(signed_transfer("carol", 40, 1)));
}
TEST_F(SignedTransactionFixture, ReplayRejectsChangedRecipientEvenWithValidProof) {
    const auto tx = signed_transfer("bob", 40, 0);
    chain.submit(tx);
    chain.mine_pending("miner");
    auto forged = chain.blocks();
    const auto& original = forged.back();
    auto transactions = original.transactions();
    transactions[0] = Transaction(wallet.address(), "carol", tx.amount(), tx.nonce(), tx.public_key(), tx.signature());
    Block altered(original.height(), original.previous_hash(), original.timestamp(), original.difficulty(), std::move(transactions));
    altered.mine();
    forged.back() = std::move(altered);
    EXPECT_FALSE(Blockchain::validate_chain(forged, {1, 50}));
}
TEST_F(SignedTransactionFixture, RestoredChainRejectsReplayAndAcceptsNextNonce) {
    const auto tx = signed_transfer("bob", 40, 0);
    chain.submit(tx);
    chain.mine_pending("miner");
    const auto directory = std::filesystem::temp_directory_path() / ("corecoin-transaction-" + key.address());
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    ChainStore store(directory / "chain.dat");
    store.save(chain);
    auto restored = store.load();
    EXPECT_EQ(restored.balance("bob"), 40);
    EXPECT_EQ(restored.next_nonce(wallet.address()), 1u);
    EXPECT_THROW(restored.submit(tx), std::invalid_argument);
    EXPECT_NO_THROW(restored.submit(signed_transfer("bob", 20, 1)));
    restored.mine_pending("miner");
    EXPECT_TRUE(restored.validate());
    EXPECT_EQ(restored.balance("bob"), 60);
}
TEST_F(SignedTransactionFixture, PendingTransfersAreNotRestoredFromConfirmedBlocks) {
    chain.submit(signed_transfer("bob", 40, 0));
    auto restored = Blockchain::from_blocks(chain.blocks(), chain.config());
    EXPECT_TRUE(restored.pending().empty());
    EXPECT_EQ(restored.balance(wallet.address()), 100);
    EXPECT_EQ(restored.available_balance(wallet.address()), 100);
    EXPECT_EQ(restored.next_nonce(wallet.address()), 0u);
    EXPECT_NO_THROW(restored.submit(signed_transfer("bob", 40, 0)));
}
TEST(TransactionTests, SerializationSeparatesFieldsAndCommitsToSignature) {
    EXPECT_NE(Transaction("ab", "c", 1, 0).id(), Transaction("a", "bc", 1, 0).id());
    const auto key = SigningKey::generate();
    const auto pub = key.public_key();
    const Transaction a(key.address(), "bob", 1, 0, pub, std::string(128, '0'));
    const Transaction b(key.address(), "bob", 1, 0, pub, std::string(128, '1'));
    EXPECT_EQ(a.signing_payload(), b.signing_payload());
    EXPECT_NE(a.id(), b.id());
}
TEST(TransactionTests, RejectsInvalidAmountsAndSelfTransfers) {
    Blockchain chain({1, 50});
    EXPECT_THROW(chain.submit(Transaction("founder", "bob", 0, 0)), std::invalid_argument);
    EXPECT_THROW(chain.submit(Transaction("founder", "bob", -1, 0)), std::invalid_argument);
    EXPECT_THROW(chain.submit(Transaction("founder", "founder", 1, 0)), std::invalid_argument);
    EXPECT_THROW(chain.submit(Transaction("founder", "bob", std::numeric_limits<Amount>::max(), 0)), std::invalid_argument);
    EXPECT_EQ(chain.available_balance("founder"), initial_wallet_allocation);
    EXPECT_EQ(chain.next_nonce("founder"), 0u);
}
TEST(TransactionTests, PendingPoolLimitRejectsExtraTransferWithoutChangingState) {
    Blockchain chain({1, 50});
    for (std::uint64_t nonce = 0; nonce < 1000; ++nonce)
        chain.submit(Transaction("founder", "bob", 1, nonce));
    ASSERT_EQ(chain.pending().size(), 1000u);
    EXPECT_THROW(chain.submit(Transaction("founder", "bob", 1, 1000)), std::length_error);
    EXPECT_EQ(chain.next_nonce("founder"), 1000u);
    EXPECT_EQ(chain.available_balance("bob"), 1000);
    chain.mine_pending("miner");
    EXPECT_EQ(chain.balance("bob"), 1000);
    EXPECT_TRUE(chain.pending().empty());
    EXPECT_TRUE(chain.validate());
    EXPECT_NO_THROW(chain.submit(Transaction("founder", "bob", 1, 1000)));
}
}
