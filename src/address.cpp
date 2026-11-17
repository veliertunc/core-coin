#include "address.hpp"
#include <algorithm>
namespace corecoin::__detail {
bool valid_account(std::string_view account) {
    return !account.empty() && account.size() <= 64 &&
        std::ranges::all_of(account, [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
}
}
