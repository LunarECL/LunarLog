#ifndef LUNAR_LOG_SOURCE_HPP
#define LUNAR_LOG_SOURCE_HPP

#include "core/log_entry.hpp"
#include "core/log_common.hpp"
#include "log_manager.hpp"
#include "sink/console_sink.hpp"
#include "formatter/human_readable_formatter.hpp"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <set>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <climits>
#include <cmath>
#include <sstream>

namespace minta {
    class LunarLog {
    public:
        explicit LunarLog(LogLevel minLevel = LogLevel::INFO, bool addDefaultConsoleSink = true)
            : m_minLevel(minLevel)
            , m_isRunning(true)
            , m_rateLimitWindowStart(std::chrono::steady_clock::now().time_since_epoch().count())
            , m_logCount(0)
            , m_captureSourceLocation(false)
            , m_hasCustomContext(false)
            , m_sinkWriteInProgress(false)
            , m_templateCacheSize(128) {
            if (addDefaultConsoleSink) {
                addSink<ConsoleSink>();
            }
            m_logThread = std::thread(&LunarLog::processLogQueue, this);
        }

        // NOTE: LunarLog must outlive all logging threads. Destroying LunarLog
        // while other threads are still calling log methods is undefined behavior.
        ~LunarLog() noexcept {
            flush();
            m_isRunning = false;
            m_logCV.notify_one();
            if (m_logThread.joinable()) {
                m_logThread.join();
            }
        }

        LunarLog(const LunarLog &) = delete;
        LunarLog &operator=(const LunarLog &) = delete;
        LunarLog(LunarLog &&) = delete;
        LunarLog &operator=(LunarLog &&) = delete;

        void setMinLevel(LogLevel level) {
            m_minLevel.store(level, std::memory_order_relaxed);
        }

        LogLevel getMinLevel() const {
            return m_minLevel.load(std::memory_order_relaxed);
        }

        void setCaptureSourceLocation(bool capture) {
            m_captureSourceLocation.store(capture, std::memory_order_relaxed);
        }

        bool getCaptureSourceLocation() const {
            return m_captureSourceLocation.load(std::memory_order_relaxed);
        }

        /// @deprecated Use setCaptureSourceLocation instead.
        inline void setCaptureContext(bool capture) {
            setCaptureSourceLocation(capture);
        }

        /// @deprecated Use getCaptureSourceLocation instead.
        inline bool getCaptureContext() const {
            return getCaptureSourceLocation();
        }

        /// @warning Do not call flush() from within a sink write() implementation.
        ///          flush() acquires the queue mutex and waits for sink writes to
        ///          complete, so calling it from inside write() will deadlock.
        void flush() {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_flushCV.wait(lock, [this] {
                return m_logQueue.empty() && !m_sinkWriteInProgress.load(std::memory_order_relaxed);
            });
        }

        template<typename SinkType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value>::type
        addSink(Args &&... args) {
            m_logManager.addSink(detail::make_unique<SinkType>(std::forward<Args>(args)...));
        }

        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value && std::is_base_of<IFormatter, FormatterType>::value>::type
        addSink(Args &&... args) {
            auto sink = detail::make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            m_logManager.addSink(std::move(sink));
        }

        void addCustomSink(std::unique_ptr<ISink> sink) {
            m_logManager.addSink(std::move(sink));
        }

        template<typename... Args>
        void log(LogLevel level, const std::string &messageTemplate, const Args &... args) {
            logInternal(level, "", 0, "", messageTemplate, args...);
        }

        template<typename... Args>
        void logWithSourceLocation(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            logInternal(level, file, line, function, messageTemplate, args...);
        }

        /// @deprecated Use logWithSourceLocation instead.
        template<typename... Args>
        void logWithContext(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            logWithSourceLocation(level, file, line, function, messageTemplate, args...);
        }

        template<typename... Args>
        void trace(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::TRACE, messageTemplate, args...);
        }

        template<typename... Args>
        void debug(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::DEBUG, messageTemplate, args...);
        }

        template<typename... Args>
        void info(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::INFO, messageTemplate, args...);
        }

        template<typename... Args>
        void warn(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::WARN, messageTemplate, args...);
        }

        template<typename... Args>
        void error(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::ERROR, messageTemplate, args...);
        }

        template<typename... Args>
        void fatal(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::FATAL, messageTemplate, args...);
        }

        void setContext(const std::string& key, const std::string& value) {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext[key] = value;
            m_hasCustomContext.store(true, std::memory_order_release);
        }

        void clearContext(const std::string& key) {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.erase(key);
            m_hasCustomContext.store(!m_customContext.empty(), std::memory_order_release);
        }

        void clearAllContext() {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.clear();
            m_hasCustomContext.store(false, std::memory_order_release);
        }

        void setTemplateCacheSize(size_t size) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_templateCacheSize = size;
            if (size == 0) {
                m_templateCache.clear();
            }
        }

    private:
        static constexpr size_t kRateLimitMaxLogs = 1000;
        static constexpr long long kRateLimitWindowSeconds = 1;

        std::atomic<LogLevel> m_minLevel;
        std::atomic<bool> m_isRunning;
        std::atomic<long long> m_rateLimitWindowStart;
        std::atomic<size_t> m_logCount;
        std::mutex m_queueMutex;
        std::mutex m_contextMutex;
        std::condition_variable m_logCV;
        std::condition_variable m_flushCV;
        std::queue<LogEntry> m_logQueue;
        std::thread m_logThread;
        LogManager m_logManager;
        std::map<std::string, std::string> m_customContext;
        std::atomic<bool> m_captureSourceLocation;
        std::atomic<bool> m_hasCustomContext;
        std::atomic<bool> m_sinkWriteInProgress;

        struct PlaceholderInfo {
            std::string name;
            std::string fullContent;
            std::string spec;
            size_t startPos;
            size_t endPos;
            char operator_;  // '@' (destructure), '$' (stringify), or 0 (none)
        };

        std::mutex m_cacheMutex;
        std::unordered_map<uint32_t, std::vector<PlaceholderInfo>> m_templateCache;
        size_t m_templateCacheSize;

        static std::vector<PlaceholderInfo> extractPlaceholders(const std::string &messageTemplate) {
            std::vector<PlaceholderInfo> placeholders;
            for (size_t i = 0; i < messageTemplate.length(); ++i) {
                if (messageTemplate[i] == '{') {
                    if (i + 1 < messageTemplate.length() && messageTemplate[i + 1] == '{') {
                        ++i;
                        continue;
                    }
                    size_t endPos = messageTemplate.find('}', i);
                    if (endPos == std::string::npos) break;
                    std::string content = messageTemplate.substr(i + 1, endPos - i - 1);
                    char op = 0;
                    std::string nameContent = content;
                    if (!content.empty() && (content[0] == '@' || content[0] == '$')) {
                        op = content[0];
                        // NOTE: content.substr(1) allocates a new string on each call.
                        // This is acceptable for typical placeholder counts (< 10 per
                        // template).  A C++17 string_view would eliminate this copy.
                        nameContent = content.substr(1);
                        // Per MessageTemplates spec: operator with empty name ({@}, {$}),
                        // double operator ({@@x}), mixed operators ({@$x}), or
                        // non-identifier start ({@ }) are all invalid — treat as literal.
                        if (nameContent.empty() || nameContent[0] == '@' || nameContent[0] == '$'
                            || !(std::isalnum(static_cast<unsigned char>(nameContent[0])) || nameContent[0] == '_')) {
                            i = endPos;
                            continue;
                        }
                    }
                    auto parts = splitPlaceholder(nameContent);
                    placeholders.push_back({parts.first, content, parts.second, i, endPos, op});
                    i = endPos;
                } else if (messageTemplate[i] == '}') {
                    if (i + 1 < messageTemplate.length() && messageTemplate[i + 1] == '}') {
                        ++i;
                    }
                }
            }
            return placeholders;
        }

        template<typename... Args>
        void logInternal(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            if (!m_isRunning.load(std::memory_order_acquire)) return;
            if (level < m_minLevel.load(std::memory_order_relaxed)) return;
            if (!rateLimitCheck()) return;

            auto now = std::chrono::system_clock::now();

            std::vector<std::string> values{toString(args)...};

            uint32_t hash = detail::fnv1a(messageTemplate);
            std::vector<PlaceholderInfo> placeholders;
            bool cacheHit = false;
            {
                std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
                if (m_templateCacheSize > 0) {
                    auto it = m_templateCache.find(hash);
                    if (it != m_templateCache.end()) {
                        placeholders = it->second;
                        cacheHit = true;
                    }
                }
            }
            if (!cacheHit) {
                placeholders = extractPlaceholders(messageTemplate);
                std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
                if (m_templateCacheSize > 0) {
                    if (m_templateCache.size() >= m_templateCacheSize) {
                        m_templateCache.clear();
                    }
                    m_templateCache[hash] = placeholders;
                }
            }

            std::vector<std::string> warnings = validatePlaceholders(messageTemplate, placeholders, values);
            std::string message = formatMessage(messageTemplate, placeholders, values);
            // Both representations are kept: arguments is simple name-value pairs
            // for backward compat; properties is the richer form with operator context.
            auto argumentPairs = mapArgumentsToPlaceholders(placeholders, values);
            auto properties = mapProperties(placeholders, values);

            bool captureCtx = m_captureSourceLocation.load(std::memory_order_relaxed);
            std::map<std::string, std::string> contextCopy;
            if (m_hasCustomContext.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> contextLock(m_contextMutex);
                contextCopy = m_customContext;
            }

            std::unique_lock<std::mutex> lock(m_queueMutex);
            // Fields must match LogEntry declaration order (aggregate init):
            // level, message, timestamp, templateStr, templateHash, arguments,
            // file, line, function, customContext, properties
            m_logQueue.emplace(LogEntry{
                level, std::move(message), now, messageTemplate, hash, std::move(argumentPairs),
                captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "", std::move(contextCopy),
                std::move(properties)
            });

            for (const auto& warning : warnings) {
                uint32_t warnHash = detail::fnv1a(warning);
                m_logQueue.emplace(LogEntry{LogLevel::WARN, warning, now, warning, warnHash, {},
                                            captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "", {}, {}});
            }

            lock.unlock();
            m_logCV.notify_one();
        }

        void processLogQueue() {
            while (m_isRunning) {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_logCV.wait(lock, [this] { return !m_logQueue.empty() || !m_isRunning; });

                while (!m_logQueue.empty()) {
                    auto entry = std::move(m_logQueue.front());
                    m_logQueue.pop();
                    m_sinkWriteInProgress.store(true, std::memory_order_relaxed);
                    lock.unlock();

                    try {
                        m_logManager.log(entry);
                    } catch (...) {
                        // Swallow — logging must not crash the application
                    }

                    // Re-acquire lock BEFORE clearing sinkWriteInProgress and
                    // notifying, so that flush() cannot miss the state change.
                    // Without this, flush() can check the predicate (seeing
                    // sinkWriteInProgress==true), then we set it to false and
                    // notify, and THEN flush() enters wait — a classic lost wakeup.
                    lock.lock();
                    m_sinkWriteInProgress.store(false, std::memory_order_relaxed);
                    m_flushCV.notify_all();
                }
            }

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                while (!m_logQueue.empty()) {
                    auto entry = std::move(m_logQueue.front());
                    m_logQueue.pop();
                    m_sinkWriteInProgress.store(true, std::memory_order_relaxed);
                    lock.unlock();
                    try {
                        m_logManager.log(entry);
                    } catch (...) {
                        // Swallow — logging must not crash the application
                    }
                    lock.lock();
                    m_sinkWriteInProgress.store(false, std::memory_order_relaxed);
                    m_flushCV.notify_all();
                }
            }
        }

        // Rate limiting is best-effort under concurrent access.
        //
        // When the window expires, the first thread to win the CAS resets
        // m_rateLimitWindowStart and stores m_logCount to 1 (counting its own
        // message). Concurrent threads that already read the old window but
        // lose the CAS retry and see the new window; their subsequent
        // fetch_add on m_logCount races with the store(1), so a small number
        // of messages at the window boundary may be silently lost or allowed
        // beyond the limit. This is acceptable for a best-effort rate limiter.
        bool rateLimitCheck() {
            auto now = std::chrono::steady_clock::now();
            long long nowNs = now.time_since_epoch().count();
            long long windowStart = m_rateLimitWindowStart.load(std::memory_order_relaxed);

            for (;;) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::duration(nowNs - windowStart)).count();
                if (duration < kRateLimitWindowSeconds) break;
                if (m_rateLimitWindowStart.compare_exchange_weak(windowStart, nowNs,
                        std::memory_order_relaxed, std::memory_order_relaxed)) {
                    // Release so that the new count is visible to threads that
                    // subsequently read it with acquire in fetch_add below.
                    m_logCount.store(1, std::memory_order_release);
                    return true;
                }
            }
            // Acquire pairs with the release-store above: if a window reset just
            // happened, this thread sees the updated count before incrementing.
            size_t count = m_logCount.fetch_add(1, std::memory_order_acquire);
            if (count >= kRateLimitMaxLogs) {
                return false;
            }
            return true;
        }

        static bool isWhitespaceOnly(const std::string &s) {
            for (size_t i = 0; i < s.size(); ++i) {
                if (!std::isspace(static_cast<unsigned char>(s[i]))) return false;
            }
            return !s.empty();
        }

        static std::vector<std::string> validatePlaceholders(
            const std::string &templateStr,
            const std::vector<PlaceholderInfo> &placeholders,
            const std::vector<std::string> &values) {
            std::vector<std::string> warnings;
            std::set<std::string> uniquePlaceholders;

            for (const auto &ph : placeholders) {
                if (ph.name.empty()) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has empty placeholder");
                } else if (isWhitespaceOnly(ph.name)) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has whitespace-only placeholder name");
                } else if (!uniquePlaceholders.insert(ph.name).second) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has duplicate placeholder name: " + ph.name);
                }
            }

            if (placeholders.size() < values.size()) {
                warnings.push_back("Warning: More values provided than placeholders");
            } else if (placeholders.size() > values.size()) {
                warnings.push_back("Warning: More placeholders than provided values");
            }

            return warnings;
        }

        template<typename T>
        static std::string toString(const T &value) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        static std::string toString(const std::string &value) {
            return value;
        }

        static std::string toString(const char *value) {
            if (!value) return "(null)";
            return std::string(value);
        }

        static std::string toString(std::nullptr_t) {
            return "(null)";
        }

        static std::string toString(int value) {
            return std::to_string(value);
        }

        static std::string toString(long value) {
            return std::to_string(value);
        }

        static std::string toString(long long value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned int value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned long value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned long long value) {
            return std::to_string(value);
        }

        static std::string toString(bool value) {
            return value ? "true" : "false";
        }

        static std::string toString(double value) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", value);
            // Locale-safety: some locales use ',' as decimal separator;
            // ensure consistent '.' for downstream parsers (e.g. JSON).
            for (char *p = buf; *p; ++p) { if (*p == ',') *p = '.'; }
            return std::string(buf);
        }

        static std::string toString(float value) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(value));
            // Locale-safety: same as toString(double).
            for (char *p = buf; *p; ++p) { if (*p == ',') *p = '.'; }
            return std::string(buf);
        }

        /// Safely parse an integer from a string. Returns fallback on failure.
        static int safeStoi(const std::string &s, int fallback = 0) {
            if (s.empty()) return fallback;
            for (size_t i = 0; i < s.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(s[i]))) return fallback;
            }
            try { return std::stoi(s); } catch (...) { return fallback; }
        }

        /// Try to parse a string as a double. Returns true on success and sets out.
        /// Single strtod call replaces the old isNumericString+parseDouble pair.
        static bool tryParseDouble(const std::string &s, double &out) {
            if (s.empty()) return false;
            const char* start = s.c_str();
            char* end = nullptr;
            errno = 0;
            double val = std::strtod(start, &end);
            if (errno == ERANGE || end == start || static_cast<size_t>(end - start) != s.size()) {
                errno = 0;
                return false;
            }
            errno = 0;
            if (std::isinf(val) || std::isnan(val)) return false;
            out = val;
            return true;
        }

        /// Kept for backward compat / simple cases.
        static bool isNumericString(const std::string &s) {
            double ignored;
            return tryParseDouble(s, ignored);
        }

        static double parseDouble(const std::string &s) {
            double val = 0.0;
            tryParseDouble(s, val);
            return val;
        }

        static double clampForLongLong(double val) {
            if (val != val) return 0.0;
            static const double kMinLL = static_cast<double>(LLONG_MIN + 1);
            static const double kMaxLL = std::nextafter(static_cast<double>(LLONG_MAX), 0.0);
            if (val < kMinLL) return kMinLL;
            if (val > kMaxLL) return kMaxLL;
            return val;
        }

        /// Format a double into a string using the given printf format.
        /// Uses measure-first pattern to avoid truncation for extreme values.
        static std::string snprintfDouble(const char* fmt, double val) {
            int needed = std::snprintf(nullptr, 0, fmt, val);
            if (needed < 0) return std::string();
            if (static_cast<size_t>(needed) < 256) {
                char stackBuf[256];
                std::snprintf(stackBuf, sizeof(stackBuf), fmt, val);
                return std::string(stackBuf);
            }
            std::vector<char> heapBuf(static_cast<size_t>(needed) + 1);
            std::snprintf(heapBuf.data(), heapBuf.size(), fmt, val);
            return std::string(heapBuf.data());
        }

        /// Format a double with precision into a string using the given printf format.
        static std::string snprintfDoublePrecision(const char* fmt, int precision, double val) {
            int needed = std::snprintf(nullptr, 0, fmt, precision, val);
            if (needed < 0) return std::string();
            if (static_cast<size_t>(needed) < 256) {
                char stackBuf[256];
                std::snprintf(stackBuf, sizeof(stackBuf), fmt, precision, val);
                return std::string(stackBuf);
            }
            std::vector<char> heapBuf(static_cast<size_t>(needed) + 1);
            std::snprintf(heapBuf.data(), heapBuf.size(), fmt, precision, val);
            return std::string(heapBuf.data());
        }

        static std::string applyFormat(const std::string &value, const std::string &spec) {
            if (spec.empty()) return value;

            double numVal;

            // Fixed-point: .Nf (e.g. ".2f", ".4f")
            if (spec.size() >= 2 && spec[0] == '.' && spec.back() == 'f') {
                if (!tryParseDouble(value, numVal)) return value;
                std::string digits = spec.substr(1, spec.size() - 2);
                int precision = safeStoi(digits, 6);
                if (precision > 50) precision = 50;
                return snprintfDoublePrecision("%.*f", precision, numVal);
            }

            // Fixed-point shorthand: Nf (e.g. "2f", "4f")
            if (spec.size() >= 2 && spec.back() == 'f' && std::isdigit(static_cast<unsigned char>(spec[0]))) {
                if (!tryParseDouble(value, numVal)) return value;
                int precision = safeStoi(spec.substr(0, spec.size() - 1), 6);
                if (precision > 50) precision = 50;
                return snprintfDoublePrecision("%.*f", precision, numVal);
            }

            // Currency: C or c
            if (spec == "C" || spec == "c") {
                if (!tryParseDouble(value, numVal)) return value;
                if (numVal < 0) {
                    std::string formatted = snprintfDouble("%.2f", -numVal);
                    if (formatted == "0.00") return "$0.00";
                    return "-$" + formatted;
                } else {
                    std::string formatted = snprintfDouble("%.2f", numVal);
                    return "$" + formatted;
                }
            }

            // Hex: X (upper) or x (lower)
            if (spec == "X" || spec == "x") {
                if (!tryParseDouble(value, numVal)) return value;
                char buf[64];
                numVal = clampForLongLong(numVal);
                long long intVal = static_cast<long long>(numVal);
                unsigned long long uval;
                std::string result;
                if (intVal < 0) {
                    result = "-";
                    uval = 0ULL - static_cast<unsigned long long>(intVal);
                } else {
                    uval = static_cast<unsigned long long>(intVal);
                }
                if (spec == "X") {
                    std::snprintf(buf, sizeof(buf), "%llX", uval);
                } else {
                    std::snprintf(buf, sizeof(buf), "%llx", uval);
                }
                result += buf;
                return result;
            }

            // Scientific: E (upper) or e (lower)
            if (spec == "E" || spec == "e") {
                if (!tryParseDouble(value, numVal)) return value;
                char buf[64];
                if (spec == "E") {
                    std::snprintf(buf, sizeof(buf), "%E", numVal);
                } else {
                    std::snprintf(buf, sizeof(buf), "%e", numVal);
                }
                return std::string(buf);
            }

            // Percentage: P or p
            if (spec == "P" || spec == "p") {
                if (!tryParseDouble(value, numVal)) return value;
                double pct = numVal * 100.0;
                if (!std::isfinite(pct)) pct = numVal;
                return snprintfDouble("%.2f", pct) + "%";
            }

            // Zero-padded integer: 0N (e.g. "04", "08") — handle negative separately
            if (spec.size() >= 2 && spec[0] == '0' && std::isdigit(static_cast<unsigned char>(spec[1]))) {
                if (!tryParseDouble(value, numVal)) return value;
                char buf[64];
                numVal = clampForLongLong(numVal);
                int width = safeStoi(spec.substr(1), 1);
                if (width > 50) width = 50;
                long long intVal = static_cast<long long>(numVal);
                if (intVal < 0) {
                    unsigned long long absVal = 0ULL - static_cast<unsigned long long>(intVal);
                    std::snprintf(buf, sizeof(buf), "-%0*llu", width, absVal);
                } else {
                    std::snprintf(buf, sizeof(buf), "%0*lld", width, intVal);
                }
                return std::string(buf);
            }

            // Unknown spec — return value as-is
            return value;
        }

        /// Split "name:spec" into (name, spec). Returns (placeholder, "") if no colon.
        static std::pair<std::string, std::string> splitPlaceholder(const std::string &placeholder) {
            size_t colonPos = placeholder.rfind(':');
            if (colonPos == std::string::npos) {
                return std::make_pair(placeholder, std::string());
            }
            return std::make_pair(placeholder.substr(0, colonPos), placeholder.substr(colonPos + 1));
        }

        static std::string formatMessage(const std::string &messageTemplate,
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values) {
            std::string result;
            result.reserve(messageTemplate.length());
            size_t phIdx = 0;
            size_t pos = 0;

            while (pos < messageTemplate.length()) {
                if (phIdx < placeholders.size() && pos == placeholders[phIdx].startPos) {
                    if (phIdx < values.size()) {
                        result += applyFormat(values[phIdx], placeholders[phIdx].spec);
                    } else {
                        result.append(messageTemplate, pos, placeholders[phIdx].endPos - pos + 1);
                    }
                    pos = placeholders[phIdx].endPos + 1;
                    ++phIdx;
                } else if (messageTemplate[pos] == '{' && pos + 1 < messageTemplate.length() && messageTemplate[pos + 1] == '{') {
                    result += '{';
                    pos += 2;
                } else if (messageTemplate[pos] == '}' && pos + 1 < messageTemplate.length() && messageTemplate[pos + 1] == '}') {
                    result += '}';
                    pos += 2;
                } else {
                    size_t litStart = pos;
                    ++pos;
                    while (pos < messageTemplate.length()) {
                        if (phIdx < placeholders.size() && pos == placeholders[phIdx].startPos) break;
                        if (messageTemplate[pos] == '{' && pos + 1 < messageTemplate.length() && messageTemplate[pos + 1] == '{') break;
                        if (messageTemplate[pos] == '}' && pos + 1 < messageTemplate.length() && messageTemplate[pos + 1] == '}') break;
                        ++pos;
                    }
                    result.append(messageTemplate, litStart, pos - litStart);
                }
            }
            return result;
        }

        static std::vector<std::pair<std::string, std::string>> mapArgumentsToPlaceholders(
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values) {
            std::vector<std::pair<std::string, std::string>> argumentPairs;

            size_t valueIndex = 0;
            for (const auto &ph : placeholders) {
                if (valueIndex >= values.size()) break;
                argumentPairs.emplace_back(ph.name, values[valueIndex++]);
            }

            return argumentPairs;
        }

        /// Map placeholders to PlaceholderProperty entries for structured formatters.
        /// @param placeholders Parsed placeholder metadata (includes operator info).
        /// @param values       Positional argument strings (from toString() conversion).
        /// Properties is the richer representation: it carries operator context (@/$)
        /// that arguments (simple name-value pairs) do not, enabling formatters to
        /// choose type-aware serialization (e.g. JSON native types for @).
        /// @note Properties are populated unconditionally — even when no operators
        ///       are used — so that structured formatters (JSON, XML) always have
        ///       access to placeholder names.  The per-message overhead is a single
        ///       reserve + N moves for typical placeholder counts (< 10), which is
        ///       negligible relative to I/O.
        static std::vector<PlaceholderProperty> mapProperties(
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values) {
            std::vector<PlaceholderProperty> props;
            props.reserve(std::min(placeholders.size(), values.size()));

            size_t valueIndex = 0;
            for (const auto &ph : placeholders) {
                if (valueIndex >= values.size()) break;
                props.push_back({ph.name, values[valueIndex++], ph.operator_});
            }

            return props;
        }
    };

    class ContextScope {
    public:
        ContextScope(LunarLog& logger, const std::string& key, const std::string& value)
            : m_logger(logger), m_key(key) {
            m_logger.setContext(key, value);
        }

        ~ContextScope() noexcept {
            m_logger.clearContext(m_key);
        }

        ContextScope(const ContextScope &) = delete;
        ContextScope &operator=(const ContextScope &) = delete;
        ContextScope(ContextScope &&) = delete;
        ContextScope &operator=(ContextScope &&) = delete;

    private:
        LunarLog& m_logger;
        std::string m_key;
    };
} // namespace minta

#endif // LUNAR_LOG_SOURCE_HPP