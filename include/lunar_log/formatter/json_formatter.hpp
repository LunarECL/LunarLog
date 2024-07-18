#ifndef LUNAR_LOG_JSON_FORMATTER_HPP
#define LUNAR_LOG_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <sstream>
#include <iomanip>

namespace minta {
    class JsonFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::ostringstream json;
            json << R"({)";

            json << R"("level":")" << getLevelString(entry.level) << R"(",)";
            json << R"("timestamp":")" << formatTimestamp(entry.timestamp) << R"(",)";
            json << R"("message":")" << escapeJsonString(entry.message) << R"(")";

            for (const auto &arg : entry.arguments) {
                json << R"(,")" << escapeJsonString(arg.first) << R"(":")" << escapeJsonString(arg.second) << R"(")";
            }

            json << R"(})";
            return json.str();
        }

    private:
        static std::string escapeJsonString(const std::string &input) {
            std::ostringstream result;
            for (char c : input) {
                switch (c) {
                    case '"': result << R"(\")"; break;
                    case '\\': result << R"(\\)"; break;
                    case '\b': result << R"(\b)"; break;
                    case '\f': result << R"(\f)"; break;
                    case '\n': result << R"(\n)"; break;
                    case '\r': result << R"(\r)"; break;
                    case '\t': result << R"(\t)"; break;
                    default:
                        if ('\x00' <= c && c <= '\x1f') {
                            result << R"(\u)" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        } else {
                            result << c;
                        }
                }
            }
            return result.str();
        }
    };
} // namespace minta

#endif // LUNAR_LOG_JSON_FORMATTER_HPP