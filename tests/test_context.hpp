#pragma once
#include <stdexcept>
#include <utility>

namespace corecoin::__tests {
class TestContext {
public:
    void require(bool condition, const char* message) {
        ++checks_;
        if (!condition) throw std::runtime_error(message);
    }
    template<class F> void rejects(F&& action, const char* message) {
        bool rejected = false;
        try { std::forward<F>(action)(); } catch (const std::exception&) { rejected = true; }
        require(rejected, message);
    }
    [[nodiscard]] int checks() const noexcept { return checks_; }
private:
    int checks_ = 0;
};
}
