#include "cli/password.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace corecoin::__cli {
namespace {
class EchoGuard {
public:
    EchoGuard() {
#ifdef _WIN32
        handle_ = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(handle_, &mode_)) {
            active_ = true;
            if (!SetConsoleMode(handle_, mode_ & ~ENABLE_ECHO_INPUT)) throw std::runtime_error("Cannot disable terminal echo");
        }
#else
        if (isatty(STDIN_FILENO)) {
            if (tcgetattr(STDIN_FILENO, &mode_) != 0) throw std::runtime_error("Cannot read terminal settings");
            auto hidden = mode_;
            hidden.c_lflag &= ~ECHO;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0) throw std::runtime_error("Cannot disable terminal echo");
            active_ = true;
        }
#endif
    }
    ~EchoGuard() {
        if (active_) {
#ifdef _WIN32
            SetConsoleMode(handle_, mode_);
#else
            tcsetattr(STDIN_FILENO, TCSANOW, &mode_);
#endif
        }
    }
    [[nodiscard]] bool active() const { return active_; }
private:
    bool active_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    DWORD mode_ = 0;
#else
    termios mode_{};
#endif
};
}
std::string read_password(std::string_view prompt) {
    std::cout << prompt << std::flush;
    std::string password;
    {
        EchoGuard guard;
        if (!std::getline(std::cin, password)) throw std::runtime_error("Could not read password");
        if (guard.active()) std::cout << '\n';
    }
    return password;
}
}
