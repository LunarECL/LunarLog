#ifndef LUNAR_LOG_SOURCE_HPP
#define LUNAR_LOG_SOURCE_HPP

#include "core/log_entry.hpp"
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
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <climits>
#include <cmath>
#include <iomanip>
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
            , m_sinkWriteInProgress(false) {
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

        void flush() {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_flushCV.wait(lock, [this] {
                return m_logQueue.empty() && !m_sinkWriteInProgress.load(std::memory_order_acquire);
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
        };

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
                    auto parts = splitPlaceholder(content);
                    placeholders.push_back({parts.first, content, parts.second, i, endPos});
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

            std::vector<std::string> values{toString(args)...};

            auto placeholders = extractPlaceholders(messageTemplate);
            std::vector<std::string> warnings = validatePlaceholders(placeholders, values);

            auto now = std::chrono::system_clock::now();
            std::string message = formatMessage(messageTemplate, placeholders, values);
            auto argumentPairs = mapArgumentsToPlaceholders(placeholders, values);

            bool captureCtx = m_captureSourceLocation.load(std::memory_order_relaxed);
            std::map<std::string, std::string> contextCopy;
            if (m_hasCustomContext.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> contextLock(m_contextMutex);
                contextCopy = m_customContext;
            }

            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_logQueue.emplace(LogEntry{
                level, std::move(message), now, messageTemplate, std::move(argumentPairs),
                captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "", std::move(contextCopy)
            });

            for (const auto& warning : warnings) {
                if (!rateLimitCheck()) break;
                m_logQueue.emplace(LogEntry{LogLevel::WARN, warning, now, warning, {},
                                            captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "", {}});
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
                    m_sinkWriteInProgress.store(true, std::memory_order_release);
                    lock.unlock();

                    m_logManager.log(entry);
                    m_sinkWriteInProgress.store(false, std::memory_order_release);
                    // Notify after every individual write so that flush() wakes up
                    // promptly even when producers keep adding messages.
                    m_flushCV.notify_all();

                    lock.lock();
                }
            }

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                while (!m_logQueue.empty()) {
                    auto entry = std::move(m_logQueue.front());
                    m_logQueue.pop();
                    m_sinkWriteInProgress.store(true, std::memory_order_release);
                    lock.unlock();
                    m_logManager.log(entry);
                    m_sinkWriteInProgress.store(false, std::memory_order_release);
                    m_flushCV.notify_all();
                    lock.lock();
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
                    m_logCount.store(1, std::memory_order_relaxed);
                    return true;
                }
            }
            size_t count = m_logCount.fetch_add(1, std::memory_order_relaxed);
            if (count >= kRateLimitMaxLogs) {
                return false;
            }
            return true;
        }

        static std::vector<std::string> validatePlaceholders(
            const std::vector<PlaceholderInfo> &placeholders,
            const std::vector<std::string> &values) {
            std::vector<std::string> warnings;
            std::set<std::string> uniquePlaceholders;

            for (const auto &ph : placeholders) {
                if (ph.name.empty()) {
                    warnings.push_back("Warning: Empty placeholder found");
                } else if (!uniquePlaceholders.insert(ph.name).second) {
                    warnings.push_back("Warning: Repeated placeholder name: " + ph.name);
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
            return std::string(value);
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
            std::ostringstream oss;
            oss << std::setprecision(15) << value;
            return oss.str();
        }

        static std::string toString(float value) {
            std::ostringstream oss;
            oss << std::setprecision(9) << value;
            return oss.str();
        }

        /// Safely parse an integer from a string. Returns fallback on failure.
        static int safeStoi(const std::string &s, int fallback = 0) {
            if (s.empty()) return fallback;
            for (size_t i = 0; i < s.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(s[i]))) return fallback;
            }
            try { return std::stoi(s); } catch (...) { return fallback; }
        }

        static double parseDouble(const std::string &s) {
            if (s.empty()) return 0.0;
            const char* start = s.c_str();
            char* end = nullptr;
            errno = 0;
            double val = std::strtod(start, &end);
            if (errno == ERANGE || end == start || static_cast<size_t>(end - start) != s.size()) {
                errno = 0;
                return 0.0;
            }
            errno = 0;
            return val;
        }

        static bool isNumericString(const std::string &s) {
            if (s.empty()) return false;
            const char* start = s.c_str();
            char* end = nullptr;
            errno = 0;
            std::strtod(start, &end);
            bool ok = (errno == 0 && end != start && static_cast<size_t>(end - start) == s.size());
            errno = 0;
            return ok;
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
        /// Uses a stack buffer for small results; falls back to heap for large ones.
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

            // Fixed-point: .Nf (e.g. ".2f", ".4f")
            if (spec.size() >= 2 && spec[0] == '.' && spec.back() == 'f') {
                if (!isNumericString(value)) return value;
                double numVal = parseDouble(value);
                std::string digits = spec.substr(1, spec.size() - 2);
                int precision = safeStoi(digits, 6);
                if (precision > 50) precision = 50;
                return snprintfDoublePrecision("%.*f", precision, numVal);
            }

            // Fixed-point shorthand: Nf (e.g. "2f", "4f")
            if (spec.size() >= 2 && spec.back() == 'f' && std::isdigit(static_cast<unsigned char>(spec[0]))) {
                if (!isNumericString(value)) return value;
                double numVal = parseDouble(value);
                int precision = safeStoi(spec.substr(0, spec.size() - 1), 6);
                if (precision > 50) precision = 50;
                return snprintfDoublePrecision("%.*f", precision, numVal);
            }

            // Currency: C or c
            if (spec == "C" || spec == "c") {
                if (!isNumericString(value)) return value;
                double numVal = parseDouble(value);
                if (numVal < 0) {
                    std::string formatted = snprintfDouble("%.2f", -numVal);
                    return "-$" + formatted;
                } else {
                    std::string formatted = snprintfDouble("%.2f", numVal);
                    return "$" + formatted;
                }
            }

            // Hex: X (upper) or x (lower)
            if (spec == "X" || spec == "x") {
                if (!isNumericString(value)) return value;
                char buf[64];
                double numVal = clampForLongLong(parseDouble(value));
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
                if (!isNumericString(value)) return value;
                char buf[64];
                double numVal = parseDouble(value);
                if (spec == "E") {
                    std::snprintf(buf, sizeof(buf), "%E", numVal);
                } else {
                    std::snprintf(buf, sizeof(buf), "%e", numVal);
                }
                return std::string(buf);
            }

            // Percentage: P or p
            if (spec == "P" || spec == "p") {
                if (!isNumericString(value)) return value;
                double numVal = parseDouble(value);
                return snprintfDouble("%.2f", numVal * 100.0) + "%";
            }

            // Zero-padded integer: 0N (e.g. "04", "08") — handle negative separately
            if (spec.size() >= 2 && spec[0] == '0' && std::isdigit(static_cast<unsigned char>(spec[1]))) {
                if (!isNumericString(value)) return value;
                char buf[64];
                double numVal = clampForLongLong(parseDouble(value));
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
            size_t colonPos = placeholder.find(':');
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