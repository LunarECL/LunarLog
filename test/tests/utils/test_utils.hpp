#ifndef LUNAR_LOG_TEST_UTILS_HPP
#define LUNAR_LOG_TEST_UTILS_HPP

#include <string>
#include <vector>
#include <cstdint>

class TestUtils {
public:
    static std::string readLogFile(const std::string &filename);
    static void waitForFileContent(const std::string &filename, int maxAttempts = 10);
    static void cleanupLogFiles();

private:
    static bool fileExists(const std::string &filename);
    static std::uintmax_t getFileSize(const std::string &filename);
    static void removeFile(const std::string &filename);
};

#endif // LUNAR_LOG_TEST_UTILS_HPP