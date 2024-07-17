#ifndef LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP
#define LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <sstream>

namespace minta {
    class HumanReadableFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::ostringstream oss;
            oss << formatTimestamp(entry.timestamp) << " "
                    << "[" << getLevelString(entry.level) << "] "
                    << entry.message;
            return oss.str();
        }
    };
} // namespace minta

#endif // LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP
