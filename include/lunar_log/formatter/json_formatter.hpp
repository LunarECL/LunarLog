#ifndef LUNAR_LOG_JSON_FORMATTER_HPP
#define LUNAR_LOG_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cmath>

namespace minta {
    class JsonFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::string levelStr = getLevelString(entry.level);
            std::string tsStr = detail::formatTimestamp(entry.timestamp);
            std::string msgEsc = escapeJsonString(localizedMessage(entry));

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
                json += escapeJsonString(entry.templateStr);
                json += R"(","templateHash":")";
                json += detail::toHexString(entry.templateHash);
                json += '"';
            }

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

            if (!entry.tags.empty()) {
                json += R"(,"tags":[)";
                bool first = true;
                for (const auto &tag : entry.tags) {
                    if (!first) json += ',';
                    json += '"';
                    json += escapeJsonString(tag);
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
                    json += escapeJsonString(prop.name);
                    json += R"(":)";
                    if (prop.op == '@') {
                        json += toJsonNativeValue(prop.value);
                    } else {
                        json += '"';
                        json += escapeJsonString(prop.value);
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
                        json += escapeJsonString(prop.name);
                        json += R"(":[)";
                        bool firstT = true;
                        for (const auto &t : prop.transforms) {
                            if (!firstT) json += ',';
                            json += '"';
                            json += escapeJsonString(t);
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

    private:
        /// Attempt to emit a JSON-native value for @ (destructure) properties.
        /// Values arrive as strings (post-toString conversion), so original type
        /// info is lost. This function uses string-based heuristics: "true"/"false"
        /// become JSON booleans, numeric-looking strings become JSON numbers.
        /// Known limitation: a string argument "true" becomes boolean true, and
        /// "3.14" becomes number 3.14.  Use the $ (stringify) operator to force
        /// string representation when this coercion is undesirable.
        /// Note: nullptr/"(null)" is emitted as the string "(null)", not JSON null,
        /// since the MessageTemplates spec does not mandate null handling.
        static std::string toJsonNativeValue(const std::string &value) {
            if (value == "true" || value == "false") {
                return value;
            }

            if (value.empty()) {
                return "\"\"";
            }

            const char* start = value.c_str();
            char* end = nullptr;
            errno = 0;
            double numVal = std::strtod(start, &end);
            if (errno == 0 && end != start && static_cast<size_t>(end - start) == value.size()
                && std::isfinite(numVal)) {
                // Re-serialize from the parsed double to guarantee valid JSON.
                // strtod accepts inputs like "+42", " 42", "0x1A" which are
                // NOT valid JSON numbers, so we cannot return the original string.
                char buf[64];
                if (numVal == static_cast<double>(static_cast<long long>(numVal))
                    && std::fabs(numVal) < 1e15) {
                    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(numVal));
                } else {
                    std::snprintf(buf, sizeof(buf), "%.15g", numVal);
                }
                // Locale-safety: some locales use ',' as decimal separator.
                // Replace with '.' to guarantee valid JSON numbers.
                for (char *p = buf; *p; ++p) { if (*p == ',') *p = '.'; }
                return std::string(buf);
            }
            errno = 0;

            std::string result = "\"";
            result += escapeJsonString(value);
            result += '"';
            return result;
        }

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