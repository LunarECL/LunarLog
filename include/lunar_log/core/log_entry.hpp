#ifndef LUNAR_LOG_ENTRY_HPP
#define LUNAR_LOG_ENTRY_HPP

#include "log_level.hpp"
#include "exception_info.hpp"
#include <string>
#include <chrono>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include <thread>

namespace minta {
    struct PlaceholderProperty {
        std::string name;
        std::string value;
        char op;  // '@' (destructure), '$' (stringify), or 0 (none)
        std::vector<std::string> transforms;
    };

    struct LogEntry {
        LogLevel level = LogLevel::INFO;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string templateStr;
        uint32_t templateHash = 0;
        std::vector<std::pair<std::string, std::string>> arguments;
        std::string file;
        int line = 0;
        std::string function;
        std::map<std::string, std::string> customContext;
        std::vector<PlaceholderProperty> properties;
        std::vector<std::string> tags;
        std::string locale = "C";
        std::thread::id threadId;
        std::unique_ptr<detail::ExceptionInfo> exception;

        /// Returns true if this log entry has exception information attached.
        bool hasException() const { return exception != nullptr; }

        LogEntry() = default;
        LogEntry(LogEntry&&) = default;
        LogEntry& operator=(LogEntry&&) = default;
        LogEntry(const LogEntry&) = delete;
        LogEntry& operator=(const LogEntry&) = delete;

        /// Backward-compatible positional constructor for custom formatters.
        LogEntry(LogLevel level_, std::string message_, std::chrono::system_clock::time_point timestamp_,
                 std::string templateStr_, uint32_t templateHash_,
                 std::vector<std::pair<std::string, std::string>> arguments_,
                 std::string file_, int line_, std::string function_,
                 std::map<std::string, std::string> customContext_,
                 std::vector<PlaceholderProperty> properties_,
                 std::vector<std::string> tags_ = {},
                 std::string locale_ = "C",
                 std::thread::id threadId_ = std::thread::id())
            : level(level_), message(std::move(message_)), timestamp(timestamp_),
              templateStr(std::move(templateStr_)), templateHash(templateHash_),
              arguments(std::move(arguments_)),
              file(std::move(file_)), line(line_), function(std::move(function_)),
              customContext(std::move(customContext_)), properties(std::move(properties_)),
              tags(std::move(tags_)),
              locale(std::move(locale_)),
              threadId(threadId_) {}
    };

namespace detail {
    /// Deep-copy a LogEntry (which is move-only due to unique_ptr member).
    inline LogEntry cloneEntry(const LogEntry& src) {
        LogEntry dst(
            src.level,
            std::string(src.message),
            src.timestamp,
            std::string(src.templateStr),
            src.templateHash,
            std::vector<std::pair<std::string, std::string>>(src.arguments),
            std::string(src.file),
            src.line,
            std::string(src.function),
            std::map<std::string, std::string>(src.customContext),
            std::vector<PlaceholderProperty>(src.properties),
            std::vector<std::string>(src.tags),
            std::string(src.locale),
            src.threadId
        );
        if (src.exception) {
            std::unique_ptr<ExceptionInfo> exCopy(new ExceptionInfo());
            exCopy->type = src.exception->type;
            exCopy->message = src.exception->message;
            exCopy->chain = src.exception->chain;
            dst.exception = std::move(exCopy);
        }
        return dst;
    }
} // namespace detail

} // namespace minta

#endif // LUNAR_LOG_ENTRY_HPP