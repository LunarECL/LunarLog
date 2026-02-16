#ifndef LUNAR_LOG_ENTRY_HPP
#define LUNAR_LOG_ENTRY_HPP

#include "log_level.hpp"
#include <string>
#include <chrono>
#include <vector>
#include <map>

namespace minta {
    struct PlaceholderProperty {
        std::string name;
        std::string value;
        char op;  // '@' (destructure), '$' (stringify), or 0 (none)
    };

    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string templateStr;
        // Maintained for backward compatibility with custom formatters.
        // Prefer `properties` for new code — it carries operator context (@/$).
        std::vector<std::pair<std::string, std::string>> arguments;
        std::string file;
        int line;
        std::string function;
        std::map<std::string, std::string> customContext;
        std::vector<PlaceholderProperty> properties;
    };
} // namespace minta

#endif // LUNAR_LOG_ENTRY_HPP