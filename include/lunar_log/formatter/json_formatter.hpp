#ifndef LUNAR_LOG_JSON_FORMATTER_HPP
#define LUNAR_LOG_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <string>
#include <cstdio>

namespace minta {
    class JsonFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::string levelStr = getLevelString(entry.level);
            std::string tsStr = detail::formatTimestamp(entry.timestamp);
            std::string msgEsc = escapeJsonString(entry.message);

            std::string json;
            json.reserve(64 + levelStr.size() + tsStr.size() + msgEsc.size());
            json += R"({"level":")";
            json += levelStr;
            json += R"(","timestamp":")";
            json += tsStr;
            json += R"(","message":")";
            json += msgEsc;
            json += '"';

            if (!entry.file.empty()) {
                std::string fileEsc = escapeJsonString(entry.file);
                std::string funcEsc = escapeJsonString(entry.function);
                json += R"(,"file":")";
                json += fileEsc;
                json += R"(","line":)";
                json += std::to_string(entry.line);
                json += R"(,"function":")";
                json += funcEsc;
                json += '"';
            }

            if (!entry.customContext.empty()) {
                json += R"(,"context":{)";
                bool first = true;
                for (const auto &ctx : entry.customContext) {
                    if (!first) json += ',';
                    json += '"';
                    json += escapeJsonString(ctx.first);
                    json += R"(":")";
                    json += escapeJsonString(ctx.second);
                    json += '"';
                    first = false;
                }
                json += '}';
            }

            json += '}';
            return json;
        }

    private:
        static std::string escapeJsonString(const std::string &input) {
            std::string result;
            result.reserve(input.size());
            for (char c : input) {
                switch (c) {
                    case '"': result += R"(\")"; break;
                    case '\\': result += R"(\\)"; break;
                    case '\b': result += R"(\b)"; break;
                    case '\f': result += R"(\f)"; break;
                    case '\n': result += R"(\n)"; break;
                    case '\r': result += R"(\r)"; break;
                    case '\t': result += R"(\t)"; break;
                    default:
                        if ('\x00' <= c && c <= '\x1f') {
                            char buf[8];
                            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                            result += buf;
                        } else {
                            result += c;
                        }
                }
            }
            return result;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_JSON_FORMATTER_HPP