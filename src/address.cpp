#include "address.hpp"
#include <algorithm>
namespace corecoin::__detail {
bool valid_account(std::string_view account) {
    if (account.size() == 67 && account.starts_with("cc_"))
        return std::ranges::all_of(account.substr(3), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        });
    return !account.empty() && account.size() <= 64 &&
        std::ranges::all_of(account, [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
}
}
