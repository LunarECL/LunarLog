#ifndef LUNAR_LOG_FILE_SINK_HPP
#define LUNAR_LOG_FILE_SINK_HPP

#include "lunar_log_sink_interface.hpp"
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace minta {

class FileSink : public ILogSink {
public:
    FileSink(const std::string& filename, size_t maxFileSize = 0)
        : filename(filename), maxFileSize(maxFileSize), currentFileSize(0) {
        openFile();
    }

    void write(const LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(fileMutex);
        if (!fileStream || !fileStream->is_open()) {
            openFile();
        }

        std::string formattedMessage = entry.isJsonLogging ?
            formatJsonLogEntry(entry) : formatLogEntry(entry);
        *fileStream << formattedMessage << std::endl;
        fileStream->flush();

        currentFileSize += formattedMessage.length() + 1;
        if (maxFileSize > 0 && currentFileSize >= maxFileSize) {
            rotateLogFile();
        }
    }

private:
    std::string filename;
    size_t maxFileSize;
    size_t currentFileSize;
    std::unique_ptr<std::ofstream> fileStream;
    std::mutex fileMutex;

    void openFile() {
        fileStream = minta::make_unique<std::ofstream>(filename, std::ios::app);
        setFilePermissions(filename);
        currentFileSize = getFileSize(filename);
    }

    void rotateLogFile() {
        if (!fileStream) return;
        fileStream->close();
        std::string newFilename = filename + "." + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::rename(filename.c_str(), newFilename.c_str());
        openFile();
    }

    void setFilePermissions(const std::string& filename) {
        #ifdef _WIN32
            SECURITY_ATTRIBUTES sa;
            SECURITY_DESCRIPTOR sd;
            InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
            SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.lpSecurityDescriptor = &sd;
            sa.bInheritHandle = FALSE;
            SetFileSecurity(filename.c_str(), DACL_SECURITY_INFORMATION, &sd);
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
        #else
            struct stat st;
            if (stat(filename.c_str(), &st) == 0) {
                return static_cast<size_t>(st.st_size);
            }
        #endif
        return 0;
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
};

} // namespace minta

#endif // LUNAR_LOG_FILE_SINK_HPP