#pragma once
#include <string>
#include <string_view>
namespace corecoin::__cli {
[[nodiscard]] std::string read_password(std::string_view prompt);
}
