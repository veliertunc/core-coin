#pragma once
namespace corecoin::__tests {
class TestContext;
void crypto_tests(TestContext& context);
void blockchain_tests(TestContext& context);
void validation_tests(TestContext& context);
void supply_tests(TestContext& context);
void amount_tests(TestContext& context);
void wallet_tests(TestContext& context);
}
