#ifndef LUNAR_LOG_ENTRY_HPP
#define LUNAR_LOG_ENTRY_HPP

#include "log_level.hpp"
#include <string>
#include <chrono>
#include <vector>

namespace minta {
    struct LogEntry {
        LogLevel level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string templateStr;
        std::vector<std::pair<std::string, std::string> > arguments;
    };
} // namespace minta

#endif // LUNAR_LOG_ENTRY_HPP
