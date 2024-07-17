#ifndef LUNAR_LOG_JSON_FORMATTER_HPP
#define LUNAR_LOG_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <nlohmann/json.hpp>

namespace minta {
    class JsonFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            nlohmann::ordered_json j;
            j["level"] = getLevelString(entry.level);
            j["timestamp"] = formatTimestamp(entry.timestamp);
            j["message"] = entry.message;
            for (const auto &arg: entry.arguments) {
                j[arg.first] = arg.second;
            }
            return j.dump();
        }
    };
} // namespace minta

#endif // LUNAR_LOG_JSON_FORMATTER_HPP
