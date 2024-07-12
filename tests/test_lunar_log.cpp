#include <gtest/gtest.h>
#include <dirent.h>
#include "lunar_log.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <iostream>
#include <unistd.h>
#include <algorithm>

class LunarLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "Current working directory: " << getcwd(nullptr, 0) << std::endl;
    }

    void TearDown() override {
        std::vector<std::string> filesToRemove = {
            "test_log.txt",
            "json_log.txt",
            "level_test_log.txt",
            "rate_limit_test_log.txt",
            "test_log1.txt",
            "test_log2.txt",
            "escaped_brackets_test.txt",
            "escaped_brackets_placeholders_test.txt"
        };

        for (const auto &filename : filesToRemove) {
            if (remove(filename.c_str()) != 0) {
                std::cerr << "Error deleting file: " << filename << std::endl;
            }
        }

        const char* logFilePrefix = "rotation_test_log";
        DIR* dir;
        struct dirent* ent;

        if ((dir = opendir(".")) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                if (strncmp(ent->d_name, logFilePrefix, strlen(logFilePrefix)) == 0) {
                    if (remove(ent->d_name) != 0) {
                        std::cerr << "Error deleting file: " << ent->d_name << std::endl;
                    }
                }
            }
            closedir(dir);
        } else {
            FAIL() << "Could not open current directory.";
        }
    }

    std::string readLogFile(const std::string &filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void waitForFileContent(const std::string &filename, int maxAttempts = 10) {
        for (int i = 0; i < maxAttempts; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!readLogFile(filename).empty()) {
                return;
            }
        }
        std::cerr << "Timeout waiting for file content: " << filename << std::endl;
    }
};

#define ASSERT_LOG_CONTAINS(logContent, expected) \
    ASSERT_TRUE(logContent.find(expected) != std::string::npos) \
        << "Expected '" << expected << "' not found in log:\n" << logContent

TEST_F(LunarLogTest, BasicLogging) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    auto fileSink = logger.addSink<minta::FileSink>("test_log.txt");

    EXPECT_NO_THROW({
        logger.trace("This is a trace message");
        logger.debug("This is a debug message with a number: {number}", 42);
        logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
        logger.warn("Warning: {attempts} attempts remaining", 3);
        logger.error("Error occurred: {error}", "File not found");
        logger.fatal("Fatal error: {error}", "System crash");
    });

    waitForFileContent("test_log.txt");
    std::string logContent = readLogFile("test_log.txt");
    std::cout << "Log content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "This is a trace message");
    ASSERT_LOG_CONTAINS(logContent, "This is a debug message with a number: 42");
    ASSERT_LOG_CONTAINS(logContent, "User alice logged in from 192.168.1.1");
    ASSERT_LOG_CONTAINS(logContent, "Warning: 3 attempts remaining");
    ASSERT_LOG_CONTAINS(logContent, "Error occurred: File not found");
    ASSERT_LOG_CONTAINS(logContent, "Fatal error: System crash");
}

TEST_F(LunarLogTest, JsonLogging) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    auto fileSink = logger.addSink<minta::FileSink>("json_log.txt");
    logger.enableJsonLogging(true);

    EXPECT_NO_THROW({
        logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
    });

    waitForFileContent("json_log.txt");
    std::string logContent = readLogFile("json_log.txt");
    std::cout << "JSON Log content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "\"level\":\"INFO\"");
    ASSERT_LOG_CONTAINS(logContent, "\"message\":\"User alice logged in from 192.168.1.1\"");
    ASSERT_LOG_CONTAINS(logContent, "\"username\":\"alice\"");
    ASSERT_LOG_CONTAINS(logContent, "\"ip_address\":\"192.168.1.1\"");
}

TEST_F(LunarLogTest, LogLevels) {
    minta::LunarLog logger(minta::LogLevel::WARN);
    auto fileSink = logger.addSink<minta::FileSink>("level_test_log.txt");

    logger.trace("This should not be logged");
    logger.debug("This should not be logged");
    logger.info("This should not be logged");
    logger.warn("This warning should be logged");
    logger.error("This error should be logged");
    logger.fatal("This fatal error should be logged");

    waitForFileContent("level_test_log.txt");
    std::string logContent = readLogFile("level_test_log.txt");
    std::cout << "Log Levels content:\n" << logContent << std::endl;

    EXPECT_TRUE(logContent.find("This should not be logged") == std::string::npos);
    ASSERT_LOG_CONTAINS(logContent, "This warning should be logged");
    ASSERT_LOG_CONTAINS(logContent, "This error should be logged");
    ASSERT_LOG_CONTAINS(logContent, "This fatal error should be logged");
}

TEST_F(LunarLogTest, RateLimiting) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {i}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    waitForFileContent("rate_limit_test_log.txt");
    std::string logContent = readLogFile("rate_limit_test_log.txt");
    std::cout << "Rate Limiting content:\n" << logContent << std::endl;

    size_t messageCount = std::count(logContent.begin(), logContent.end(), '\n');
    EXPECT_EQ(messageCount, 1001);
    ASSERT_LOG_CONTAINS(logContent, "This message should appear after the rate limit reset");
}

TEST_F(LunarLogTest, FileRotation) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("rotation_test_log.txt", 100);

    for (int i = 0; i < 100; ++i) {
        logger.info("This is a long message to test file rotation: {i}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_TRUE(std::ifstream("rotation_test_log.txt").good()) << "rotation_test_log.txt should exist";

    bool rotatedFileExists = false;
    DIR* dir;
    struct dirent* ent;
    const char* logFilePrefix = "rotation_test_log.";

    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, logFilePrefix, strlen(logFilePrefix)) == 0) {
                rotatedFileExists = true;
                std::cout << "Found rotated file: " << ent->d_name << std::endl;
                break;
            }
        }
        closedir(dir);
    } else {
        FAIL() << "Could not open current directory.";
    }

    EXPECT_TRUE(rotatedFileExists) << "A rotated log file should exist";
}

TEST_F(LunarLogTest, EmptyPlaceholder) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("This message has an empty placeholder: {}", 1);

    waitForFileContent("test_log.txt");
    std::string logContent = readLogFile("test_log.txt");
    std::cout << "Empty Placeholder content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "This message has an empty placeholder:");
    ASSERT_LOG_CONTAINS(logContent, "Warning: Empty placeholder found");
}

TEST_F(LunarLogTest, RepeatedPlaceholder) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("This message has a repeated placeholder: {repeat} and {repeat}", "value1", "value2");

    waitForFileContent("test_log.txt");
    std::string logContent = readLogFile("test_log.txt");
    std::cout << "Repeated Placeholder content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "This message has a repeated placeholder: value1 and value2");
    ASSERT_LOG_CONTAINS(logContent, "Warning: Repeated placeholder name: repeat");
}

TEST_F(LunarLogTest, MismatchedPlaceholdersAndValues) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Too few values: {a} {b} {c}", "value1", "value2");
    logger.info("Too many values: {a}", "value1", "value2");

    waitForFileContent("test_log.txt");
    std::string logContent = readLogFile("test_log.txt");
    std::cout << "Mismatched Placeholders content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "Too few values: value1 value2");
    ASSERT_LOG_CONTAINS(logContent, "Warning: More placeholders than provided values");
    ASSERT_LOG_CONTAINS(logContent, "Too many values: value1");
    ASSERT_LOG_CONTAINS(logContent, "Warning: More values provided than placeholders");
}

TEST_F(LunarLogTest, MultipleSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink1 = logger.addSink<minta::FileSink>("test_log1.txt");
    auto fileSink2 = logger.addSink<minta::FileSink>("test_log2.txt");

    logger.info("This message should appear in both logs");

    waitForFileContent("test_log1.txt");
    waitForFileContent("test_log2.txt");
    std::string logContent1 = readLogFile("test_log1.txt");
    std::string logContent2 = readLogFile("test_log2.txt");

    ASSERT_LOG_CONTAINS(logContent1, "This message should appear in both logs");
    ASSERT_LOG_CONTAINS(logContent2, "This message should appear in both logs");
}

TEST_F(LunarLogTest, RemoveSink) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink1 = logger.addSink<minta::FileSink>("test_log1.txt");
    auto fileSink2 = logger.addSink<minta::FileSink>("test_log2.txt");

    logger.info("This message should appear in both logs");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    logger.removeSink(fileSink2);
    logger.info("This message should appear only in log1");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string logContent1 = readLogFile("test_log1.txt");
    std::string logContent2 = readLogFile("test_log2.txt");

    ASSERT_LOG_CONTAINS(logContent1, "This message should appear in both logs");
    ASSERT_LOG_CONTAINS(logContent1, "This message should appear only in log1");
    ASSERT_LOG_CONTAINS(logContent2, "This message should appear in both logs");
    EXPECT_TRUE(logContent2.find("This message should appear only in log1") == std::string::npos);
}

TEST_F(LunarLogTest, DefaultConsoleSink) {
    testing::internal::CaptureStdout();

    {
        minta::LunarLog logger(minta::LogLevel::INFO);
        logger.info("This message should appear in console");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::string output = testing::internal::GetCapturedStdout();
    std::cout << "Captured output: " << output << std::endl;
    ASSERT_TRUE(output.find("This message should appear in console") != std::string::npos);
}

TEST_F(LunarLogTest, EscapedBrackets) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("escaped_brackets_test.txt");

    logger.info("This message has escaped brackets: {{escaped}}");
    logger.info("This message has a mix of escaped and unescaped brackets: {{escaped}} and {unescaped}", "value");
    logger.info("This message has double escaped brackets: {{{{double_escaped}}}}");
    logger.info("This message has an escaped opening bracket: {{open and a closing bracket}", "value");
    logger.info("This message has an opening bracket and an escaped closing bracket: {open}} and {unescaped}", "value1", "value2");

    waitForFileContent("escaped_brackets_test.txt");
    std::string logContent = readLogFile("escaped_brackets_test.txt");
    std::cout << "Escaped Brackets content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "This message has escaped brackets: {escaped}");
    ASSERT_LOG_CONTAINS(logContent, "This message has a mix of escaped and unescaped brackets: {escaped} and value");
    ASSERT_LOG_CONTAINS(logContent, "This message has double escaped brackets: {{double_escaped}}");
    ASSERT_LOG_CONTAINS(logContent, "This message has an escaped opening bracket: {open and a closing bracket}");
    ASSERT_LOG_CONTAINS(logContent, "This message has an opening bracket and an escaped closing bracket: value1} and value2");
}

TEST_F(LunarLogTest, EscapedBracketsWithPlaceholders) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    auto fileSink = logger.addSink<minta::FileSink>("escaped_brackets_placeholders_test.txt");

    logger.info("Escaped brackets don't count as placeholders: {{name}}", "value");
    logger.info("Mixed escaped and unescaped: {{escaped}} {unescaped} {{another_escaped}}", "value");
    logger.info("Escaped brackets next to placeholders: {{}}{}{{}} {}", "value1", "value2");

    waitForFileContent("escaped_brackets_placeholders_test.txt");
    std::string logContent = readLogFile("escaped_brackets_placeholders_test.txt");
    std::cout << "Escaped Brackets with Placeholders content:\n" << logContent << std::endl;

    ASSERT_LOG_CONTAINS(logContent, "Escaped brackets don't count as placeholders: {name}");
    ASSERT_LOG_CONTAINS(logContent, "Mixed escaped and unescaped: {escaped} value {another_escaped}");
    ASSERT_LOG_CONTAINS(logContent, "Escaped brackets next to placeholders: {}value1{} value2");
}