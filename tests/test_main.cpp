#include "module.hpp"
#include <iostream>
#include <exception>

int main() {
    corecoin::__tests::TestContext context;
    try {
        corecoin::__tests::crypto_tests(context);
        corecoin::__tests::blockchain_tests(context);
        corecoin::__tests::validation_tests(context);
        corecoin::__tests::supply_tests(context);
        corecoin::__tests::amount_tests(context);
        corecoin::__tests::wallet_tests(context);
        std::cout << context.checks() << " checks passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Check " << context.checks() << " failed: " << ex.what() << '\n';
        return 1;
    }
}
