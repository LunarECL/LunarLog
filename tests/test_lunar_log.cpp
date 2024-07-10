#include <gtest/gtest.h>
#include <dirent.h>
#include "lunar_log.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

class LunarLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Clean up test log files
        std::vector<std::string> filesToRemove = {
            "test_log.txt",
            "json_log.txt",
            "level_test_log.txt",
            "rate_limit_test_log.txt",
        };

        // Remove the specific log files
        for (const auto &filename : filesToRemove) {
            std::remove(filename.c_str());
        }

        // Remove all files starting with 'rotation_test_log'
        const char* logFilePrefix = "rotation_test_log";

        DIR* dir;
        struct dirent* ent;

        if ((dir = opendir(".")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (strncmp(ent->d_name, logFilePrefix, strlen(logFilePrefix)) == 0) {
                    std::remove(ent->d_name);
                }
            }
            closedir(dir);
        } else {
            // Could not open directory
            FAIL() << "Could not open current directory.";
        }
    }

    std::string readLogFile(const std::string &filename) {
        std::ifstream file(filename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

TEST_F(LunarLogTest, BasicLogging) {
    minta::LunarLog logger(minta::LunarLog::Level::TRACE, "test_log.txt");

    EXPECT_NO_THROW({
        logger.trace("This is a trace message");
        logger.debug("This is a debug message with a number: {number}", 42);
        logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
        logger.warn("Warning: {attempts} attempts remaining", 3);
        logger.error("Error occurred: {error}", "File not found");
        logger.fatal("Fatal error: {error}", "System crash");
        });

    std::string logContent = readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("This is a trace message") != std::string::npos);
    EXPECT_TRUE(logContent.find("This is a debug message with a number: 42") != std::string::npos);
    EXPECT_TRUE(logContent.find("User alice logged in from 192.168.1.1") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: 3 attempts remaining") != std::string::npos);
    EXPECT_TRUE(logContent.find("Error occurred: File not found") != std::string::npos);
    EXPECT_TRUE(logContent.find("Fatal error: System crash") != std::string::npos);
}

TEST_F(LunarLogTest, JsonLogging) {
    minta::LunarLog logger(minta::LunarLog::Level::TRACE, "json_log.txt");
    logger.enableJsonLogging(true);

    EXPECT_NO_THROW({
        logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
        });

    std::string logContent = readLogFile("json_log.txt");
    EXPECT_TRUE(logContent.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"User alice logged in from 192.168.1.1\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"username\":\"alice\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"ip_address\":\"192.168.1.1\"") != std::string::npos);
}

TEST_F(LunarLogTest, LogLevels) {
    minta::LunarLog logger(minta::LunarLog::Level::WARN, "level_test_log.txt");

    logger.trace("This should not be logged");
    logger.debug("This should not be logged");
    logger.info("This should not be logged");
    logger.warn("This warning should be logged");
    logger.error("This error should be logged");
    logger.fatal("This fatal error should be logged");

    std::string logContent = readLogFile("level_test_log.txt");
    EXPECT_TRUE(logContent.find("This should not be logged") == std::string::npos);
    EXPECT_TRUE(logContent.find("This warning should be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This error should be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This fatal error should be logged") != std::string::npos);
}

TEST_F(LunarLogTest, RateLimiting) {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {i}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    std::string logContent = readLogFile("rate_limit_test_log.txt");
    size_t messageCount = std::count(logContent.begin(), logContent.end(), '\n');
    EXPECT_EQ(messageCount, 1001);
    EXPECT_TRUE(logContent.find("This message should appear after the rate limit reset") != std::string::npos);
}

TEST_F(LunarLogTest, FileRotation) {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "rotation_test_log.txt", 100);
    // Small max file size for testing

    for (int i = 0; i < 100; ++i) {
        logger.info("This is a long message to test file rotation: {i}", i);
    }

    // Check if original file exists and has content
    EXPECT_TRUE(std::ifstream("rotation_test_log.txt").good());

    // Check if at least one rotated file exists
    bool rotatedFileExists = false;
    DIR* dir;
    struct dirent* ent;
    const char* logFilePrefix = "rotation_test_log.txt.";

    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, logFilePrefix, strlen(logFilePrefix)) == 0) {
                // Check if the suffix is a valid timestamp
                std::string suffix = ent->d_name + strlen(logFilePrefix);
                if (std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
                    rotatedFileExists = true;
                    std::remove(ent->d_name);
                    break;
                }
            }
        }
        closedir(dir);
    } else {
        // Could not open directory
        FAIL() << "Could not open current directory.";
    }

    EXPECT_TRUE(rotatedFileExists);
}


TEST_F(LunarLogTest, EmptyPlaceholder) {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "test_log.txt");

    logger.info("This message has an empty placeholder: {}", 1);

    std::string logContent = readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("This message has an empty placeholder:") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: Empty placeholder found") != std::string::npos);
}

TEST_F(LunarLogTest, RepeatedPlaceholder) {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "test_log.txt");

    logger.info("This message has a repeated placeholder: {repeat} and {repeat}", "value1", "value2");

    std::string logContent = readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("This message has a repeated placeholder: value1 and value2") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: Repeated placeholder name: repeat") != std::string::npos);
}

TEST_F(LunarLogTest, MismatchedPlaceholdersAndValues) {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "test_log.txt");

    logger.info("Too few values: {a} {b} {c}", "value1", "value2");
    logger.info("Too many values: {a}", "value1", "value2");

    std::string logContent = readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("Too few values: value1 value2") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: More placeholders than provided values") != std::string::npos);
    EXPECT_TRUE(logContent.find("Too many values: value1") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: More values provided than placeholders") != std::string::npos);
}
