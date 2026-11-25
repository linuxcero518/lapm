#include "lapm/logger.hpp"

namespace lapm {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

std::string_view Logger::prefix(LogLevel lvl) noexcept {
    switch (lvl) {
        case LogLevel::Debug: return "[debug] ";
        case LogLevel::Info:  return "[ info] ";
        case LogLevel::Warn:  return "[ warn] ";
        case LogLevel::Error: return "[error] ";
    }
    return "";
}

} // namespace lapm
