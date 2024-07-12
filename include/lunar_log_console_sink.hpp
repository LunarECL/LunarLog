#ifndef LUNAR_LOG_CONSOLE_SINK_HPP
#define LUNAR_LOG_CONSOLE_SINK_HPP

#include "lunar_log_sink_interface.hpp"
#include <iostream>

namespace minta {

    class ConsoleSink : public ILogSink {
    public:
        void write(const LogEntry& entry) override {
            std::cout << formatLogEntry(entry) << std::endl;
        }

    private:
        static std::string formatLogEntry(const LogEntry& entry) {
            std::ostringstream oss;
            oss << formatTimestamp(entry.timestamp) << " "
                << "[" << getLevelString(entry.level) << "] "
                << entry.message;
            return oss.str();
        }
    };

} // namespace minta

#endif // LUNAR_LOG_CONSOLE_SINK_HPP