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
#include <regex>
#include <type_traits>
#include <set>
#include <map>

namespace minta {
    class LunarLog {
    public:
        explicit LunarLog(LogLevel minLevel = LogLevel::INFO)
            : m_minLevel(minLevel)
            , m_isRunning(true)
            , m_lastLogTime(std::chrono::steady_clock::now())
            , m_logCount(0)
            , m_captureContext(false) {
            addSink<ConsoleSink>();
            m_logThread = std::thread(&LunarLog::processLogQueue, this);
        }

        ~LunarLog() {
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

        void setCaptureContext(bool capture) {
            m_captureContext.store(capture, std::memory_order_relaxed);
        }

        bool getCaptureContext() const {
            return m_captureContext.load(std::memory_order_relaxed);
        }

        template<typename SinkType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value>::type
        addSink(Args &&... args) {
            auto sink = make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(make_unique<HumanReadableFormatter>());
            m_logManager.addSink(std::move(sink));
        }

        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value && std::is_base_of<IFormatter, FormatterType>::value>::type
        addSink(Args &&... args) {
            auto sink = make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(make_unique<FormatterType>());
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
        void logWithContext(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            logInternal(level, file, line, function, messageTemplate, args...);
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
        }

        void clearContext(const std::string& key) {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.erase(key);
        }

        void clearAllContext() {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.clear();
        }

    private:
        static constexpr size_t kRateLimitMaxLogs = 1000;
        static constexpr long long kRateLimitWindowSeconds = 1;

        std::atomic<LogLevel> m_minLevel;
        std::atomic<bool> m_isRunning;
        std::chrono::steady_clock::time_point m_lastLogTime;
        size_t m_logCount;
        std::mutex m_queueMutex;
        std::mutex m_contextMutex;
        std::mutex m_rateLimitMutex;
        std::condition_variable m_logCV;
        std::queue<LogEntry> m_logQueue;
        std::thread m_logThread;
        LogManager m_logManager;
        std::map<std::string, std::string> m_customContext;
        std::atomic<bool> m_captureContext;

        static const std::regex& placeholderRegex() {
            static const std::regex instance(R"(\{([^{}]*)\})");
            return instance;
        }

        template<typename... Args>
        void logInternal(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            if (level < m_minLevel.load(std::memory_order_relaxed)) return;
            if (!rateLimitCheck()) return;

            auto validationResult = validatePlaceholders(messageTemplate, args...);
            std::string validatedTemplate = validationResult.first;
            std::vector<std::string> warnings = validationResult.second;

            auto now = std::chrono::system_clock::now();
            std::string message = formatMessage(validatedTemplate, args...);
            auto argumentPairs = mapArgumentsToPlaceholders(validatedTemplate, args...);

            bool captureCtx = m_captureContext.load(std::memory_order_relaxed);
            std::map<std::string, std::string> contextCopy;
            {
                std::lock_guard<std::mutex> contextLock(m_contextMutex);
                contextCopy = m_customContext;
            }

            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_logQueue.emplace(LogEntry{
                level, std::move(message), now, validatedTemplate, std::move(argumentPairs),
                captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "", std::move(contextCopy)
            });

            for (const auto& warning : warnings) {
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
                    lock.unlock();

                    m_logManager.log(entry);

                    lock.lock();
                }
            }
        }

        bool rateLimitCheck() {
            std::lock_guard<std::mutex> lock(m_rateLimitMutex);
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastLogTime).count();
            if (duration >= kRateLimitWindowSeconds) {
                m_lastLogTime = now;
                m_logCount = 1;
                return true;
            }
            if (m_logCount >= kRateLimitMaxLogs) {
                return false;
            }
            ++m_logCount;
            return true;
        }

        template<typename... Args>
        static std::pair<std::string, std::vector<std::string>> validatePlaceholders(
            const std::string &messageTemplate, const Args &... args) {
            std::vector<std::string> warnings;
            std::vector<std::string> placeholders;
            std::set<std::string> uniquePlaceholders;
            std::vector<std::string> values{toString(args)...};

            const std::regex& regex = placeholderRegex();
            auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), regex);
            auto placeholderEnd = std::sregex_iterator();

            for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd; ++i) {
                std::smatch match = *i;
                std::string placeholder = match[1].str();

                if (placeholder.empty()) {
                    std::string prefix = match.prefix().str();
                    if (!prefix.empty() && prefix.back() == '{') {
                        continue;
                    }
                }

                placeholders.push_back(placeholder);

                if (placeholder.empty()) {
                    warnings.push_back("Warning: Empty placeholder found");
                } else if (!uniquePlaceholders.insert(placeholder).second) {
                    warnings.push_back("Warning: Repeated placeholder name: " + placeholder);
                }
            }

            if (placeholders.size() < values.size()) {
                warnings.push_back("Warning: More values provided than placeholders");
            } else if (placeholders.size() > values.size()) {
                warnings.push_back("Warning: More placeholders than provided values");
            }

            return std::make_pair(messageTemplate, warnings);
        }

        template<typename T>
        static std::string toString(const T &value) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        template<typename... Args>
        static std::string formatMessage(const std::string &messageTemplate, const Args &... args) {
            std::vector<std::string> values{toString(args)...};
            std::string result;
            result.reserve(messageTemplate.length());
            size_t valueIndex = 0;

            for (size_t i = 0; i < messageTemplate.length(); ++i) {
                if (messageTemplate[i] == '{') {
                    if (i + 1 < messageTemplate.length() && messageTemplate[i + 1] == '{') {
                        result += '{';
                        ++i;
                    } else {
                        size_t endPos = messageTemplate.find("}", i);
                        if (endPos == std::string::npos) {
                            result += messageTemplate[i];
                        } else if (valueIndex < values.size()) {
                            result += values[valueIndex++];
                            i = endPos;
                        } else {
                            result += messageTemplate.substr(i, endPos - i + 1);
                            i = endPos;
                        }
                    }
                } else if (messageTemplate[i] == '}') {
                    if (i + 1 < messageTemplate.length() && messageTemplate[i + 1] == '}') {
                        result += '}';
                        ++i;
                    } else {
                        result += messageTemplate[i];
                    }
                } else {
                    result += messageTemplate[i];
                }
            }
            return result;
        }

        template<typename... Args>
        static std::vector<std::pair<std::string, std::string>> mapArgumentsToPlaceholders(
            const std::string &messageTemplate, const Args &... args) {
            std::vector<std::pair<std::string, std::string>> argumentPairs;
            std::vector<std::string> values{toString(args)...};

            const std::regex& regex = placeholderRegex();
            auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), regex);
            auto placeholderEnd = std::sregex_iterator();

            size_t valueIndex = 0;
            for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd && valueIndex < values.size(); ++i) {
                std::smatch match = *i;
                std::string placeholder = match[1].str();

                if (placeholder.empty()) {
                    std::string prefix = match.prefix().str();
                    if (!prefix.empty() && prefix.back() == '{') {
                        continue;
                    }
                }

                argumentPairs.emplace_back(placeholder, values[valueIndex++]);
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

        ~ContextScope() {
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