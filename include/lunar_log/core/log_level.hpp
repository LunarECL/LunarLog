#ifndef LUNAR_LOG_LEVEL_HPP
#define LUNAR_LOG_LEVEL_HPP

// Windows.h defines ERROR as 0, which conflicts with the ERROR enum member.
// Undefine it here so LogLevel::ERROR is always usable regardless of include order.
#ifdef ERROR
#undef ERROR
#endif

namespace minta {
    enum class LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    inline const char *getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }
} // namespace minta

#endif // LUNAR_LOG_LEVEL_HPP
