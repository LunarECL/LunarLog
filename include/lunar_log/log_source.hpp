#ifndef LUNAR_LOG_SOURCE_HPP
#define LUNAR_LOG_SOURCE_HPP

#include "core/log_common.hpp"
#include "core/log_entry.hpp"
#include "log_manager.hpp"
#include "sink/console_sink.hpp"
#include "sink/file_sink.hpp"
#include "formatter/human_readable_formatter.hpp"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <type_traits>
#include <set>

namespace minta {
    class LunarLog {
    public:
        LunarLog(LogLevel minLevel = LogLevel::INFO)
            : m_minLevel(minLevel)
              , m_isRunning(true)
              , m_lastLogTime(std::chrono::steady_clock::now())
              , m_logCount(0) {
            // Default configuration
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
            m_minLevel = level;
        }

        LogLevel getMinLevel() const {
            return m_minLevel;
        }

        template<typename SinkType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value>::type
        addSink(Args &&... args) {
            auto sink = make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(make_unique<HumanReadableFormatter>());
            m_logManager.addSink(std::move(sink));
        }

        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<std::is_base_of<ISink, SinkType>::value && std::is_base_of<IFormatter,
                                    FormatterType>::value>::type
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
            if (level < m_minLevel) return;
            if (!rateLimitCheck()) return;

            auto validationResult = validatePlaceholders(messageTemplate, args...);
            std::string validatedTemplate = validationResult.first;
            std::vector<std::string> warnings = validationResult.second;

            auto now = std::chrono::system_clock::now();
            std::string message = formatMessage(validatedTemplate, args...);
            auto argumentPairs = mapArgumentsToPlaceholders(validatedTemplate, args...);

            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_logQueue.emplace(LogEntry{
                level, std::move(message), now, validatedTemplate, std::move(argumentPairs), false
            });

            for (const auto &warning: warnings) {
                m_logQueue.emplace(LogEntry{LogLevel::WARN, warning, now, warning, {}, false});
            }

            lock.unlock();
            m_logCV.notify_one();
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

    private:
        LogLevel m_minLevel;
        std::atomic<bool> m_isRunning;
        std::chrono::steady_clock::time_point m_lastLogTime;
        std::atomic<size_t> m_logCount;
        std::mutex m_queueMutex;
        std::condition_variable m_logCV;
        std::queue<LogEntry> m_logQueue;
        std::thread m_logThread;
        LogManager m_logManager;

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
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastLogTime).count();
            if (duration >= 1) {
                m_lastLogTime = now;
                m_logCount = 1;
                return true;
            }
            if (m_logCount >= 1000) {
                return false;
            }
            ++m_logCount;
            return true;
        }

        template<typename... Args>
        static std::pair<std::string, std::vector<std::string> > validatePlaceholders(
            const std::string &messageTemplate, const Args &... args) {
            std::vector<std::string> warnings;
            std::vector<std::string> placeholders;
            std::set<std::string> uniquePlaceholders;
            std::vector<std::string> values{toString(args)...};

            std::regex placeholderRegex(R"(\{([^{}]*)\})");
            auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(),
                                                         placeholderRegex);
            auto placeholderEnd = std::sregex_iterator();

            for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd; ++i) {
                std::smatch match = *i;
                std::string placeholder = match[1].str();

                if (placeholder.empty() && match.prefix().str().back() == '{') {
                    continue;
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
        static std::vector<std::pair<std::string, std::string> > mapArgumentsToPlaceholders(
            const std::string &messageTemplate, const Args &... args) {
            std::vector<std::pair<std::string, std::string> > argumentPairs;
            std::vector<std::string> values{toString(args)...};

            std::regex placeholderRegex(R"(\{([^{}]*)\})");
            auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(),
                                                         placeholderRegex);
            auto placeholderEnd = std::sregex_iterator();

            size_t valueIndex = 0;
            for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd && valueIndex < values.size(); ++i) {
                std::smatch match = *i;
                std::string placeholder = match[1].str();

                if (placeholder.empty() && match.prefix().str().back() == '{') {
                    continue;
                }

                argumentPairs.emplace_back(placeholder, values[valueIndex++]);
            }

            return argumentPairs;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_SOURCE_HPP
