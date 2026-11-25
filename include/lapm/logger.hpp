#pragma once

#include <cstdio>
#include <format>
#include <iostream>
#include <string_view>
#include <syncstream>

namespace lapm {

enum class LogLevel { Debug = 0, Info, Warn, Error };

// Thread-safe logger writing to stderr.
// Uses std::format for messages and std::osyncstream for output atomicity.
class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel lvl) noexcept { level_ = lvl; }
    [[nodiscard]] LogLevel level() const noexcept { return level_; }
    [[nodiscard]] bool enabled(LogLevel lvl) const noexcept {
        return static_cast<int>(lvl) >= static_cast<int>(level_);
    }

    template <class... Args>
    void log(LogLevel lvl, std::format_string<Args...> fmt, Args&&... args) {
        if (!enabled(lvl)) return;
        std::osyncstream out{std::cerr};
        out << prefix(lvl)
            << std::format(fmt, std::forward<Args>(args)...)
            << '\n';
    }

private:
    Logger() = default;
    [[nodiscard]] static std::string_view prefix(LogLevel lvl) noexcept;

    LogLevel level_ = LogLevel::Info;
};

template <class... Args>
void log_debug(std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
}
template <class... Args>
void log_info(std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().log(LogLevel::Info, fmt, std::forward<Args>(args)...);
}
template <class... Args>
void log_warn(std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().log(LogLevel::Warn, fmt, std::forward<Args>(args)...);
}
template <class... Args>
void log_error(std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().log(LogLevel::Error, fmt, std::forward<Args>(args)...);
}

} // namespace lapm
