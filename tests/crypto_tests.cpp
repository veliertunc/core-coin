#include "module.hpp"
#include "../src/module.hpp"
namespace corecoin::__tests {
void crypto_tests(TestContext& context) {
    auto require = [&](bool value, const char* message) { context.require(value, message); };
    using corecoin::__detail::sha256;
    require(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA empty vector");
    require(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA abc vector");
    require(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "SHA padding vector");
    require(sha256(std::string(1'000'000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "SHA multi-block vector");
    require(Transaction("ab", "c", 1, 0).id() != Transaction("a", "bc", 1, 0).id(), "Unambiguous encoding");
}
}
