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
        std::vector<std::string> transforms;
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
        /// Tags parsed from [bracketed] prefixes in the message template.
        std::vector<std::string> tags;
        /// The locale used when formatting this entry's message.
        /// Formatters with a different per-sink locale can re-render
        /// via detail::reformatMessage using the raw values in properties.
        std::string locale;

        LogEntry(LogLevel level_, std::string message_, std::chrono::system_clock::time_point timestamp_,
                 std::string templateStr_, uint32_t templateHash_,
                 std::vector<std::pair<std::string, std::string>> arguments_,
                 std::string file_, int line_, std::string function_,
                 std::map<std::string, std::string> customContext_,
                 std::vector<PlaceholderProperty> properties_,
                 std::vector<std::string> tags_ = {},
                 std::string locale_ = "C")
            : level(level_), message(std::move(message_)), timestamp(timestamp_),
              templateStr(std::move(templateStr_)), templateHash(templateHash_),
              arguments(std::move(arguments_)),
              file(std::move(file_)), line(line_), function(std::move(function_)),
              customContext(std::move(customContext_)), properties(std::move(properties_)),
              tags(std::move(tags_)),
              locale(std::move(locale_)) {}
    };
} // namespace minta

#endif // LUNAR_LOG_ENTRY_HPP