#ifndef LUNAR_LOG_H
#define LUNAR_LOG_H

#include <nlohmann/json.hpp>
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
#include <iomanip>
#include <set>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace minta {

#if __cplusplus < 201402L
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

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

    explicit LunarLog(Level minLevel = Level::INFO, const std::string& filename = "", size_t maxFileSize = 0, bool jsonLogging = false)
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
        fileStream = make_unique<std::ofstream>(filename, std::ios::app);
        setFilePermissions(filename);
        currentFileSize = getFileSize(filename);
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
static std::pair<std::string, std::vector<std::string>> validatePlaceholders(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::string> warnings;
        std::vector<std::string> placeholders;
        std::set<std::string> uniquePlaceholders;
        std::vector<std::string> values{toString(args)...};

        std::regex placeholderRegex(R"(\{([^}]*)\})");
        auto placeholderBegin = std::sregex_iterator(messageTemplate.begin(), messageTemplate.end(), placeholderRegex);
        auto placeholderEnd = std::sregex_iterator();

        for (std::sregex_iterator i = placeholderBegin; i != placeholderEnd; ++i) {
            std::smatch match = *i;
            std::string placeholder = match[1].str();
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

    template<typename... Args>
void log(Level level, const std::string& messageTemplate, const Args&... args) {
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
            logQueue.emplace(LogEntry{Level::WARN, warning, now, warning, {}, this->jsonLogging});
        }

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
                    if (maxFileSize > 0 && currentFileSize >= maxFileSize) {
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
        std::rename(logFilename.c_str(), newFilename.c_str());
        fileStream->open(logFilename, std::ios::app);
        setFilePermissions(logFilename);
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

        for (const auto& arg : entry.arguments) {
            jsonEntry[arg.first] = arg.second;
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
        return toStringImpl(value, 0);
    }

    template<typename T>
    static auto toStringImpl(const T& value, int) -> decltype(std::to_string(value)) {
        return std::to_string(value);
    }

    template<typename T>
    static auto toStringImpl(const T& value, long) -> decltype(std::string(value)) {
        return std::string(value);
    }

    template<typename T>
    static std::string toStringImpl(const T& value, ...) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    template<typename... Args>
    static std::string formatMessage(const std::string& messageTemplate, const Args&... args) {
        std::vector<std::string> values{toString(args)...};
        std::string result = messageTemplate;
        size_t pos = 0;
        size_t valueIndex = 0;
        while ((pos = result.find("{", pos)) != std::string::npos) {
            size_t endPos = result.find("}", pos);
            if (endPos == std::string::npos) break;
            if (valueIndex < values.size()) {
                result.replace(pos, endPos - pos + 1, values[valueIndex]);
                pos += values[valueIndex].length();
                valueIndex++;
            } else {
                pos = endPos + 1;
            }
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

    void setFilePermissions(const std::string& filename) {
    #ifdef _WIN32
        PACL pACL = NULL;
        EXPLICIT_ACCESS ea;

        ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
        ea.grfAccessPermissions = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
        ea.grfAccessMode = SET_ACCESS;
        ea.grfInheritance = NO_INHERITANCE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
        ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea.Trustee.ptstrName = (LPTSTR)"CURRENT_USER";

        if (SetEntriesInAcl(1, &ea, NULL, &pACL) == ERROR_SUCCESS) {
            SetNamedSecurityInfo((LPSTR)filename.c_str(), SE_FILE_OBJECT,
                                 DACL_SECURITY_INFORMATION, NULL, NULL, pACL, NULL);
            LocalFree(pACL);
        }
    #else
        chmod(filename.c_str(), S_IRUSR | S_IWUSR);
    #endif
    }

    size_t getFileSize(const std::string& filename) {
    #ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesEx(filename.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER size;
            size.HighPart = fad.nFileSizeHigh;
            size.LowPart = fad.nFileSizeLow;
            return static_cast<size_t>(size.QuadPart);
        }
        return 0;
    #else
        struct stat st;
        if (stat(filename.c_str(), &st) == 0) {
            return static_cast<size_t>(st.st_size);
        }
        return 0;
    #endif
    }
};

} // namespace minta

#endif // LUNAR_LOG_H