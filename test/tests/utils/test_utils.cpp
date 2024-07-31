#include "test_utils.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

std::string TestUtils::readLogFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void TestUtils::waitForFileContent(const std::string &filename, int maxAttempts) {
    for (int i = 0; i < maxAttempts; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (fileExists(filename) && getFileSize(filename) > 0) {
            return;
        }
    }
    throw std::runtime_error("Timeout waiting for file content: " + filename);
}

void TestUtils::cleanupLogFiles() {
    std::vector<std::string> filesToRemove = {
        "test_log.txt", "level_test_log.txt", "rate_limit_test_log.txt",
        "escaped_brackets_test.txt", "test_log1.txt", "test_log2.txt",
        "validation_test_log.txt", "custom_formatter_log.txt", "json_formatter_log.txt", "xml_formatter_log.txt",
        "context_test_log.txt", "default_formatter_log.txt"
    };

    for (const auto &filename : filesToRemove) {
        removeFile(filename);
    }
}

bool TestUtils::fileExists(const std::string &filename) {
#if __cplusplus >= 201703L
    return fs::exists(filename);
#else
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
#endif
}

std::uintmax_t TestUtils::getFileSize(const std::string &filename) {
#if __cplusplus >= 201703L
    return fs::file_size(filename);
#else
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) != 0) {
        return 0;
    }
    return buffer.st_size;
#endif
}

void TestUtils::removeFile(const std::string &filename) {
#if __cplusplus >= 201703L
    fs::remove(filename);
#else
    std::remove(filename.c_str());
#endif
}