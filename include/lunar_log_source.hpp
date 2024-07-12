#ifndef LUNAR_LOG_SOURCE_HPP
#define LUNAR_LOG_SOURCE_HPP

#include "lunar_log_common.hpp"
#include "lunar_log_sink_interface.hpp"
#include "lunar_log_console_sink.hpp"
#include <memory>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <regex>
#include <set>

namespace minta {

class LunarLog {
public:
    LunarLog(LogLevel minLevel = LogLevel::INFO)
        : minLevel(minLevel)
        , jsonLogging(false)
        , isRunning(true)
        , lastLogTime(std::chrono::steady_clock::now())
        , logCount(0)
    {
        addSink(minta::make_unique<ConsoleSink>());
        logThread = std::thread(&LunarLog::processLogQueue, this);
    }

    ~LunarLog() {
        isRunning = false;
        logCV.notify_one();
        if (logThread.joinable()) {
            logThread.join();
        }
    }

    LunarLog(const LunarLog&) = delete;
    LunarLog& operator=(const LunarLog&) = delete;
    LunarLog(LunarLog&&) = delete;
    LunarLog& operator=(LunarLog&&) = delete;

    void setMinLevel(LogLevel level) {
        minLevel = level;
    }

    LogLevel getMinLevel() const {
        return minLevel;
    }

    void enableJsonLogging(bool enable) {
        jsonLogging = enable;
    }

    void addSink(std::unique_ptr<ILogSink> sink) {
        std::lock_guard<std::mutex> lock(sinkMutex);
        sinks.push_back(std::move(sink));
    }

    void removeSink(ILogSink* sink) {
        std::lock_guard<std::mutex> lock(sinkMutex);
        sinks.erase(std::remove_if(sinks.begin(), sinks.end(),
            [sink](const std::unique_ptr<ILogSink>& p) { return p.get() == sink; }),
            sinks.end());
    }

    template<typename... Args>
    void log(LogLevel level, const std::string& messageTemplate, const Args&... args) {
        if (level < minLevel) return;
        if (!rateLimitCheck()) return;

        auto validationResult = validatePlaceholders(messageTemplate, args...);
        std::string validatedTemplate = validationResult.first;
        std::vector<std::string> warnings = validationResult.second;

        auto now = std::chrono::system_clock::now();
        std::string message = formatMessage(validatedTemplate, args...);
        auto argumentPairs = mapArgumentsToPlaceholders(validatedTemplate, args...);

        std::unique_lock<std::mutex> lock(queueMutex);
        logQueue.emplace(LogEntry{level, std::move(message), now, validatedTemplate, std::move(argumentPairs), this->jsonLogging});

        for (const auto& warning : warnings) {
            logQueue.emplace(LogEntry{LogLevel::WARN, warning, now, warning, {}, this->jsonLogging});
        }

        lock.unlock();
        logCV.notify_one();
    }

    template<typename... Args>
    void trace(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::TRACE, messageTemplate, args...);
    }

    template<typename... Args>
    void debug(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::DEBUG, messageTemplate, args...);
    }

    template<typename... Args>
    void info(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::INFO, messageTemplate, args...);
    }

    template<typename... Args>
    void warn(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::WARN, messageTemplate, args...);
    }

    template<typename... Args>
    void error(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::ERROR, messageTemplate, args...);
    }

    template<typename... Args>
    void fatal(const std::string& messageTemplate, const Args&... args) {
        log(LogLevel::FATAL, messageTemplate, args...);
    }

private:
    LogLevel minLevel;
    bool jsonLogging;
    std::atomic<bool> isRunning;
    std::chrono::steady_clock::time_point lastLogTime;
    std::atomic<size_t> logCount;
    std::mutex queueMutex;
    std::mutex sinkMutex;
    std::condition_variable logCV;
    std::queue<LogEntry> logQueue;
    std::thread logThread;
    std::vector<std::unique_ptr<ILogSink>> sinks;

    void processLogQueue() {
        while (isRunning) {
            std::unique_lock<std::mutex> lock(queueMutex);
            logCV.wait(lock, [this] { return !logQueue.empty() || !isRunning; });

            while (!logQueue.empty()) {
                auto entry = std::move(logQueue.front());
                logQueue.pop();
                lock.unlock();

                std::lock_guard<std::mutex> sinkLock(sinkMutex);
                for (const auto& sink : sinks) {
                    sink->write(entry);
                }

                lock.lock();
            }
        }
    }

    bool rateLimitCheck() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count();
        if (duration >= 1) {
            lastLogTime = now;
            logCount = 1;
            return true;
        }
        if (logCount >= 1000) {
            return false;
        }
        ++logCount;
        return true;
    }

    template<typename... Args>
    static std::pair<std::string, std::vector<std::string>> validatePlaceholders(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::string> warnings;
        std::vector<std::string> placeholders;
        std::set<std::string> uniquePlaceholders;
        std::vector<std::string> values{toString(args)...};

        std::regex placeholderRegex(R"(\{([^{}]*)\})");
        auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), placeholderRegex);
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
    static std::string toString(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    template<typename... Args>
    static std::string formatMessage(const std::string& messageTemplate, const Args&... args) {
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
    static std::vector<std::pair<std::string, std::string>> mapArgumentsToPlaceholders(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::pair<std::string, std::string>> argumentPairs;
        std::vector<std::string> values{toString(args)...};

        std::regex placeholderRegex(R"(\{([^{}]*)\})");
        auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), placeholderRegex);
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