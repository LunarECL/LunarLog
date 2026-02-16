#ifndef LUNAR_LOG_ENTRY_HPP
#define LUNAR_LOG_ENTRY_HPP

#include "log_level.hpp"
#include <string>
#include <chrono>
#include <cstdint>
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
        uint32_t templateHash;
        // Maintained for backward compatibility with custom formatters.
        // Prefer `properties` for new code — it carries operator context (@/$).
        std::vector<std::pair<std::string, std::string>> arguments;
        std::string file;
        int line;
        std::string function;
        std::map<std::string, std::string> customContext;
        std::vector<PlaceholderProperty> properties;

        LogEntry(LogLevel level_, std::string message_, std::chrono::system_clock::time_point timestamp_,
                 std::string templateStr_, uint32_t templateHash_,
                 std::vector<std::pair<std::string, std::string>> arguments_,
                 std::string file_, int line_, std::string function_,
                 std::map<std::string, std::string> customContext_,
                 std::vector<PlaceholderProperty> properties_)
            : level(level_), message(std::move(message_)), timestamp(timestamp_),
              templateStr(std::move(templateStr_)), templateHash(templateHash_),
              arguments(std::move(arguments_)),
              file(std::move(file_)), line(line_), function(std::move(function_)),
              customContext(std::move(customContext_)), properties(std::move(properties_)) {}
    };
} // namespace minta

#endif // LUNAR_LOG_ENTRY_HPP