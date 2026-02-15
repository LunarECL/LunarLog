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
            oss << detail::formatTimestamp(entry.timestamp) << " "
                << "[" << getLevelString(entry.level) << "] "
                << entry.message;

            if (!entry.file.empty()) {
                oss << " [" << entry.file << ":" << entry.line << " " << entry.function << "]";
            }

            if (!entry.customContext.empty()) {
                oss << " {";
                bool first = true;
                for (const auto &ctx : entry.customContext) {
                    if (!first) oss << ", ";
                    oss << ctx.first << "=" << ctx.second;
                    first = false;
                }
                oss << "}";
            }

            return oss.str();
        }
    };
} // namespace minta

#endif // LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP