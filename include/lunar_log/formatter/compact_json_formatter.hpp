#ifndef LUNAR_LOG_COMPACT_JSON_FORMATTER_HPP
#define LUNAR_LOG_COMPACT_JSON_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "json_detail.hpp"
#include "../core/log_common.hpp"
#include <string>
#include <cstdio>
#include <atomic>

namespace minta {
    /// Compact JSON formatter producing single-line JSONL output optimized for
    /// log pipelines (ELK, Datadog, Loki).  Uses short property names with @
    /// prefix following the CLEF (Compact Log Event Format) convention.
    ///
    /// System fields:
    ///   @t  = timestamp (ISO 8601, UTC, ms precision)
    ///   @l  = level (3-char: TRC, DBG, INF, WRN, ERR, FTL; omitted for INFO)
    ///   @mt = message template
    ///   @m  = rendered message (optional, off by default)
    ///   @i  = template hash (included when template is present)
    ///   @x  = exception info (type: message, with nested chain if present)
    ///
    /// Properties and context are flattened to top level.  User property names
    /// starting with @ are escaped to @@ to prevent collision with system fields.
    /// The @l field is omitted for INFO level (parsers assume INFO when absent).
    class CompactJsonFormatter : public IFormatter {
    public:
        CompactJsonFormatter() : m_includeRenderedMessage(false) {}

        /// Enable/disable the @m (rendered message) field.
        /// Default is false (off).  Thread-safe.
        void includeRenderedMessage(bool include) {
            m_includeRenderedMessage.store(include, std::memory_order_relaxed);
        }

        bool isRenderedMessageIncluded() const {
            return m_includeRenderedMessage.load(std::memory_order_relaxed);
        }

        std::string format(const LogEntry &entry) const override {
            std::string json;
            json.reserve(128);

            // @t - timestamp (ISO 8601, UTC, ms precision)
            json += R"({"@t":")";
            json += formatTimestampUtc(entry.timestamp);
            json += '"';

            // @l - level (omitted for INFO)
            if (entry.level != LogLevel::INFO) {
                json += R"(,"@l":")";
                json += getCompactLevel(entry.level);
                json += '"';
            }

            // @mt - message template (always present)
            json += R"(,"@mt":")";
            if (!entry.templateStr.empty()) {
                json += detail::json::escapeJsonString(entry.templateStr);
            } else {
                json += detail::json::escapeJsonString(entry.message);
            }
            json += '"';

            // @i - template hash (when template is present)
            if (!entry.templateStr.empty()) {
                json += R"(,"@i":")";
                json += detail::toHexString(entry.templateHash);
                json += '"';
            }

            // @m - rendered message (optional, off by default)
            if (m_includeRenderedMessage.load(std::memory_order_relaxed)) {
                json += R"(,"@m":")";
                json += detail::json::escapeJsonString(localizedMessage(entry));
                json += '"';
            }

            if (!entry.exceptionType.empty()) {
                // Build the full @x string first, then escape once to avoid
                // fragmented escaping that can produce inconsistent output.
                std::string xValue;
                xValue += entry.exceptionType;
                xValue += ": ";
                xValue += entry.exceptionMessage;
                if (!entry.exceptionChain.empty()) {
                    xValue += '\n';
                    xValue += entry.exceptionChain;
                }
                json += R"(,"@x":")";
                json += detail::json::escapeJsonString(xValue);
                json += '"';
            }

            for (const auto &prop : entry.properties) {
                json += ',';
                json += '"';
                json += escapePropertyName(prop.name);
                json += R"(":)";
                if (prop.op == '@') {
                    json += detail::json::toJsonNativeValue(prop.value);
                } else {
                    json += '"';
                    json += detail::json::escapeJsonString(prop.value);
                    json += '"';
                }
            }

            // Flatten context to top level
            for (const auto &ctx : entry.customContext) {
                json += ',';
                json += '"';
                json += escapePropertyName(ctx.first);
                json += R"(":)";
                json += '"';
                json += detail::json::escapeJsonString(ctx.second);
                json += '"';
            }

            // Tags
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

            json += '}';
            return json;
        }

    private:
        std::atomic<bool> m_includeRenderedMessage;

        /// Format timestamp as ISO 8601 UTC with millisecond precision.
        /// Example: "2026-02-16T12:00:00.000Z"
        static std::string formatTimestampUtc(const std::chrono::system_clock::time_point &time) {
            auto epoch = std::chrono::system_clock::to_time_t(time);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                time.time_since_epoch()) % 1000;

            std::tm tmBuf;
#if defined(_MSC_VER)
            gmtime_s(&tmBuf, &epoch);
#else
            gmtime_r(&epoch, &tmBuf);
#endif

            char buf[32];
            size_t pos = std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
            if (pos == 0) {
                buf[0] = '\0';
            }
            // Defensive: ensure non-negative milliseconds for pre-epoch time points
            std::snprintf(buf + pos, sizeof(buf) - pos, ".%03dZ",
                         static_cast<int>((ms.count() + 1000) % 1000));
            return std::string(buf);
        }

        /// Return 3-character level abbreviation.
        static const char* getCompactLevel(LogLevel level) {
            switch (level) {
                case LogLevel::TRACE: return "TRC";
                case LogLevel::DEBUG: return "DBG";
                case LogLevel::INFO:  return "INF";
                case LogLevel::WARN:  return "WRN";
                case LogLevel::ERROR: return "ERR";
                case LogLevel::FATAL: return "FTL";
                default: return "INF";
            }
        }

        /// Escape a property name for use as a JSON key.
        /// Names starting with @ are escaped to @@ to avoid collision with
        /// system fields (@t, @l, @mt, etc.).
        static std::string escapePropertyName(const std::string &name) {
            if (!name.empty() && name[0] == '@') {
                return "@" + detail::json::escapeJsonString(name);
            }
            return detail::json::escapeJsonString(name);
        }
    };
} // namespace minta

#endif // LUNAR_LOG_COMPACT_JSON_FORMATTER_HPP
