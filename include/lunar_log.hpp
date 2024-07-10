#ifndef LUNAR_LOG_H
#define LUNAR_LOG_H

#include "json.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <sstream>
#include <vector>
#include <fstream>
#include <regex>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace minta {

class LunarLog {
public:
    enum class Level {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL
    };

    explicit LunarLog(Level minLevel = Level::INFO, const std::string& filename = "", size_t maxFileSize = 10 * 1024 * 1024, bool jsonLogging = false)
        : minLevel(minLevel)
        , jsonLogging(jsonLogging)
        , logFilename(filename)
        , maxFileSize(maxFileSize)
        , currentFileSize(0)
        , lastLogTime(std::chrono::steady_clock::now())
        , logCount(0)
        , isRunning(true)
    {
        if (!filename.empty()) {
            setLogFile(filename);
        }
        logThread = std::thread(&LunarLog::processLogQueue, this);
    }

    ~LunarLog() {
        isRunning = false;
        logCV.notify_one();
        if (logThread.joinable()) {
            logThread.join();
        }
        if (fileStream) {
            fileStream->close();
        }
    }

    LunarLog(const LunarLog&) = delete;
    LunarLog& operator=(const LunarLog&) = delete;
    LunarLog(LunarLog&&) = delete;
    LunarLog& operator=(LunarLog&&) = delete;

    void setMinLevel(Level level) {
        minLevel = level;
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (fileStream) {
            fileStream->close();
        }
        fileStream = std::make_unique<std::ofstream>(filename, std::ios::app);
        std::filesystem::permissions(filename, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        currentFileSize = std::filesystem::file_size(filename);
        logFilename = filename;
    }

    void enableJsonLogging(bool enable) {
        jsonLogging = enable;
    }

    template<typename... Args>
    void trace(const std::string& messageTemplate, const Args&... args) {
        log(Level::TRACE, messageTemplate, args...);
    }

    template<typename... Args>
    void debug(const std::string& messageTemplate, const Args&... args) {
        log(Level::DEBUG, messageTemplate, args...);
    }

    template<typename... Args>
    void info(const std::string& messageTemplate, const Args&... args) {
        log(Level::INFO, messageTemplate, args...);
    }

    template<typename... Args>
    void warn(const std::string& messageTemplate, const Args&... args) {
        log(Level::WARN, messageTemplate, args...);
    }

    template<typename... Args>
    void error(const std::string& messageTemplate, const Args&... args) {
        log(Level::ERROR, messageTemplate, args...);
    }

    template<typename... Args>
    void fatal(const std::string& messageTemplate, const Args&... args) {
        log(Level::FATAL, messageTemplate, args...);
    }

private:
    struct LogEntry {
        Level level;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string templateStr;
        std::vector<std::pair<std::string, std::string>> arguments;
        bool isJsonLogging;
    };

    Level minLevel;
    bool jsonLogging;
    std::string logFilename;
    size_t maxFileSize;
    std::unique_ptr<std::ofstream> fileStream;
    std::atomic<size_t> currentFileSize;
    std::chrono::steady_clock::time_point lastLogTime;
    std::atomic<size_t> logCount{0};
    std::atomic<bool> isRunning;
    std::mutex queueMutex;
    std::mutex fileMutex;
    std::condition_variable logCV;
    std::queue<LogEntry> logQueue;
    std::thread logThread;

    template<typename... Args>
    void log(Level level, const std::string& messageTemplate, const Args&... args) {
        if (level < minLevel) return;
        if (!rateLimitCheck()) return;

        auto now = std::chrono::system_clock::now();
        std::string message = formatMessage(messageTemplate, args...);
        auto argumentPairs = mapArgumentsToPlaceholders(messageTemplate, args...);

        std::unique_lock<std::mutex> lock(queueMutex);
        logQueue.emplace(LogEntry{level, std::move(message), now, messageTemplate, std::move(argumentPairs), this->jsonLogging});
        lock.unlock();
        logCV.notify_one();
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

    void processLogQueue() {
        while (isRunning) {
            std::unique_lock<std::mutex> lock(queueMutex);
            logCV.wait(lock, [this] { return !logQueue.empty() || !isRunning; });

            while (!logQueue.empty()) {
                auto entry = std::move(logQueue.front());
                logQueue.pop();
                lock.unlock();

                std::string formattedMessage = entry.isJsonLogging ? formatJsonLogEntry(entry) : formatLogEntry(entry);

                std::cout << formattedMessage << std::endl;

                std::lock_guard<std::mutex> fileLock(fileMutex);
                if (fileStream && fileStream->is_open()) {
                    *fileStream << formattedMessage << std::endl;
                    fileStream->flush();
                    currentFileSize += formattedMessage.length() + 1;
                    if (currentFileSize >= maxFileSize) {
                        rotateLogFile();
                    }
                }

                lock.lock();
            }
        }
    }

    void rotateLogFile() {
        if (!fileStream) return;
        fileStream->close();
        std::string newFilename = logFilename + "." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::rename(logFilename, newFilename);
        fileStream->open(logFilename, std::ios::app);
        std::filesystem::permissions(logFilename, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        currentFileSize = 0;
    }

    static std::string formatLogEntry(const LogEntry& entry) {
        std::ostringstream oss;
        oss << formatTimestamp(entry.timestamp) << " "
            << "[" << getLevelString(entry.level) << "] "
            << entry.message;
        return oss.str();
    }

    static std::string formatJsonLogEntry(const LogEntry& entry) {
        nlohmann::ordered_json jsonEntry;

        jsonEntry["level"] = getLevelString(entry.level);
        jsonEntry["timestamp"] = formatTimestamp(entry.timestamp);
        jsonEntry["message"] = entry.message;

        for (const auto& [key, value] : entry.arguments) {
            jsonEntry[key] = value;
        }

        return jsonEntry.dump();
    }

    static std::string formatTimestamp(const std::chrono::system_clock::time_point& time) {
        auto nowTime = std::chrono::system_clock::to_time_t(time);
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
        return oss.str();
    }

    static const char* getLevelString(Level level) {
        switch (level) {
            case Level::TRACE: return "TRACE";
            case Level::DEBUG: return "DEBUG";
            case Level::INFO:  return "INFO";
            case Level::WARN:  return "WARN";
            case Level::ERROR: return "ERROR";
            case Level::FATAL: return "FATAL";
            default:           return "UNKNOWN";
        }
    }

    template<typename T>
    static std::string toString(const T& value) {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    }

    template<typename... Args>
    static std::string formatMessage(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::string> values{toString(args)...};
        std::string result = messageTemplate;
        size_t pos = 0;
        for (const auto& value : values) {
            pos = result.find("{", pos);
            if (pos == std::string::npos) break;
            size_t endPos = result.find("}", pos);
            if (endPos == std::string::npos) break;
            result.replace(pos, endPos - pos + 1, value);
            pos += value.length();
        }
        return result;
    }

    template<typename... Args>
    static std::vector<std::pair<std::string, std::string>> mapArgumentsToPlaceholders(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::pair<std::string, std::string>> argumentPairs;
        std::vector<std::string> values{toString(args)...};

        std::regex placeholderRegex(R"(\{([^}]+)\})");
        auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), placeholderRegex);
        auto placeholderEnd = std::sregex_iterator();

        size_t valueIndex = 0;
        for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd && valueIndex < values.size(); ++i, ++valueIndex) {
            std::smatch match = *i;
            argumentPairs.emplace_back(match[1].str(), values[valueIndex]);
        }

        return argumentPairs;
    }
};

} // namespace minta

#endif // LUNAR_LOG_H