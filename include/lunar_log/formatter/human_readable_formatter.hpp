#ifndef LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP
#define LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <string>

namespace minta {
    class HumanReadableFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::string result;
            result.reserve(80 + entry.message.size() + entry.file.size() + entry.function.size());
            result += detail::formatTimestamp(entry.timestamp);
            result += " [";
            result += getLevelString(entry.level);
            result += "] ";
            result += entry.message;

            if (!entry.file.empty()) {
                result += " [";
                result += entry.file;
                result += ':';
                result += std::to_string(entry.line);
                result += ' ';
                result += entry.function;
                result += ']';
            }

            if (!entry.customContext.empty()) {
                result += " {";
                bool first = true;
                for (const auto &ctx : entry.customContext) {
                    if (!first) result += ", ";
                    result += ctx.first;
                    result += '=';
                    // Quote values containing delimiters
                    if (ctx.second.find(',') != std::string::npos ||
                        ctx.second.find('=') != std::string::npos) {
                        result += '"';
                        result += ctx.second;
                        result += '"';
                    } else {
                        result += ctx.second;
                    }
                    first = false;
                }
                result += '}';
            }

            return result;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP