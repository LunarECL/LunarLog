#ifndef LUNAR_LOG_JSON_FORMATTER_HPP
#define LUNAR_LOG_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "json_detail.hpp"
#include "../core/log_common.hpp"
#include <string>

namespace minta {
    class JsonFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::string levelStr = getLevelString(entry.level);
            std::string tsStr = detail::formatTimestamp(entry.timestamp);
            std::string msgEsc = detail::json::escapeJsonString(localizedMessage(entry));

            std::string json;
            json.reserve(64 + levelStr.size() + tsStr.size() + msgEsc.size());
            json += R"({"level":")";
            json += levelStr;
            json += R"(","timestamp":")";
            json += tsStr;
            json += R"(","message":")";
            json += msgEsc;
            json += '"';

            if (!entry.templateStr.empty()) {
                json += R"(,"messageTemplate":")";
                json += detail::json::escapeJsonString(entry.templateStr);
                json += R"(","templateHash":")";
                json += detail::toHexString(entry.templateHash);
                json += '"';
            }

            if (!entry.file.empty()) {
                std::string fileEsc = detail::json::escapeJsonString(entry.file);
                std::string funcEsc = detail::json::escapeJsonString(entry.function);
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
                    json += detail::json::escapeJsonString(ctx.first);
                    json += R"(":")";
                    json += detail::json::escapeJsonString(ctx.second);
                    json += '"';
                    first = false;
                }
                json += '}';
            }

            if (!entry.tags.empty()) {
                json += R"(,"tags":[)";
                bool first = true;
                for (const auto &tag : entry.tags) {
                    if (!first) json += ',';
                    json += '"';
                    json += detail::json::escapeJsonString(tag);
                    json += '"';
                    first = false;
                }
                json += ']';
            }

            if (!entry.properties.empty()) {
                json += R"(,"properties":{)";
                bool first = true;
                for (const auto &prop : entry.properties) {
                    if (!first) json += ',';
                    json += '"';
                    json += detail::json::escapeJsonString(prop.name);
                    json += R"(":)";
                    if (prop.op == '@') {
                        json += detail::json::toJsonNativeValue(prop.value);
                    } else {
                        json += '"';
                        json += detail::json::escapeJsonString(prop.value);
                        json += '"';
                    }
                    first = false;
                }
                json += '}';

                bool hasTransforms = false;
                for (const auto &prop : entry.properties) {
                    if (!prop.transforms.empty()) { hasTransforms = true; break; }
                }
                if (hasTransforms) {
                    json += R"(,"transforms":{)";
                    bool firstProp = true;
                    for (const auto &prop : entry.properties) {
                        if (prop.transforms.empty()) continue;
                        if (!firstProp) json += ',';
                        json += '"';
                        json += detail::json::escapeJsonString(prop.name);
                        json += R"(":[)";
                        bool firstT = true;
                        for (const auto &t : prop.transforms) {
                            if (!firstT) json += ',';
                            json += '"';
                            json += detail::json::escapeJsonString(t);
                            json += '"';
                            firstT = false;
                        }
                        json += ']';
                        firstProp = false;
                    }
                    json += '}';
                }
            }

            json += '}';
            return json;
        }

    };
} // namespace minta

#endif // LUNAR_LOG_JSON_FORMATTER_HPP