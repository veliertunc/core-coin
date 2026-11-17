#include "module.hpp"
#include <limits>
namespace corecoin::__tests {
void amount_tests(TestContext& context) {
    auto require = [&](bool value, const char* message) { context.require(value, message); };
    auto rejects = [&](auto action, const char* message) { context.rejects(action, message); };
    require(format_amount(1) == "0.00000001 CORE", "One Gil formatting");
    require(parse_core_amount("12.00000001") == 12 * gil_per_coin + 1, "Parse decimal CORE exactly");
    rejects([] { (void)parse_core_amount("1.000000001"); }, "Reject fractional Gil");
    rejects([] { (void)parse_core_amount("92233720368.54775808"); }, "Reject CORE overflow");
    require(format_amount(std::numeric_limits<Amount>::min()) == "-92233720368.54775808 CORE", "Signed minimum formatting");}
}
