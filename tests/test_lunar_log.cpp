#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "lunar_log.hpp"

class LunarLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        cleanupLogFiles();
    }

    void TearDown() override {
        cleanupLogFiles();
    }

    void cleanupLogFiles() {
        std::vector<std::string> filesToRemove = {
            "test_log.txt", "level_test_log.txt", "rate_limit_test_log.txt",
            "escaped_brackets_test.txt", "test_log1.txt", "test_log2.txt",
            "validation_test_log.txt", "custom_formatter_log.txt", "json_formatter_log.txt", "xml_formatter_log.txt"
        };

        for (const auto &filename: filesToRemove) {
            std::filesystem::remove(filename);
        }
    }

    std::string readLogFile(const std::string &filename) {
        std::ifstream file(filename);
        EXPECT_TRUE(file.is_open()) << "Failed to open file: " << filename;
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void waitForFileContent(const std::string &filename, int maxAttempts = 10) {
        for (int i = 0; i < maxAttempts; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (std::filesystem::exists(filename) && std::filesystem::file_size(filename) > 0) {
                return;
            }
        }
        FAIL() << "Timeout waiting for file content: " << filename;
    }
};

TEST_F(LunarLogTest, BasicLogging) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.trace("This is a trace message");
    logger.debug("This is a debug message with a number: {number}", 42);
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    logger.warn("Warning: {attempts} attempts remaining", 3);
    logger.error("Error occurred: {error}", "File not found");
    logger.fatal("Fatal error: {errorType}", "System crash");

    waitForFileContent("test_log.txt");
    std::string logContent = readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("This is a trace message") != std::string::npos);
    EXPECT_TRUE(logContent.find("This is a debug message with a number: 42") != std::string::npos);
    EXPECT_TRUE(logContent.find("User alice logged in from 192.168.1.1") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: 3 attempts remaining") != std::string::npos);
    EXPECT_TRUE(logContent.find("Error occurred: File not found") != std::string::npos);
    EXPECT_TRUE(logContent.find("Fatal error: System crash") != std::string::npos);
}

TEST_F(LunarLogTest, LogLevels) {
    minta::LunarLog logger(minta::LogLevel::WARN);
    logger.addSink<minta::FileSink>("level_test_log.txt");

    logger.trace("This should not be logged");
    logger.debug("This should not be logged");
    logger.info("This should not be logged");
    logger.warn("This warning should be logged");
    logger.error("This error should be logged");
    logger.fatal("This fatal error should be logged");

    waitForFileContent("level_test_log.txt");
    std::string logContent = readLogFile("level_test_log.txt");

    EXPECT_TRUE(logContent.find("This should not be logged") == std::string::npos);
    EXPECT_TRUE(logContent.find("This warning should be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This error should be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This fatal error should be logged") != std::string::npos);
}

TEST_F(LunarLogTest, RateLimiting) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {index}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    waitForFileContent("rate_limit_test_log.txt");
    std::string logContent = readLogFile("rate_limit_test_log.txt");

    size_t messageCount = std::count(logContent.begin(), logContent.end(), '\n');
    EXPECT_EQ(messageCount, 1001);
    EXPECT_TRUE(logContent.find("This message should appear after the rate limit reset") != std::string::npos);
}

TEST_F(LunarLogTest, PlaceholderValidation) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("validation_test_log.txt");

    logger.info("Empty placeholder: {}", "value");
    logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
    logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
    logger.info("Too many values: {placeholder}", "value1", "value2");

    waitForFileContent("validation_test_log.txt");
    std::string logContent = readLogFile("validation_test_log.txt");

    EXPECT_TRUE(logContent.find("Warning: Empty placeholder found") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: Repeated placeholder name: placeholder") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: More placeholders than provided values") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: More values provided than placeholders") != std::string::npos);
}

TEST_F(LunarLogTest, EscapedBrackets) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("escaped_brackets_test.txt");

    logger.info("This message has escaped brackets: {{escaped}}");
    logger.info("This message has a mix of escaped and unescaped brackets: {{escaped}} and {placeholder}", "value");

    waitForFileContent("escaped_brackets_test.txt");
    std::string logContent = readLogFile("escaped_brackets_test.txt");

    EXPECT_TRUE(logContent.find("This message has escaped brackets: {escaped}") != std::string::npos);
    EXPECT_TRUE(
        logContent.find("This message has a mix of escaped and unescaped brackets: {escaped} and value") != std::string
        ::npos);
}

TEST_F(LunarLogTest, MultipleSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("test_log1.txt");
    logger.addSink<minta::FileSink>("test_log2.txt");

    logger.info("This message should appear in both logs");

    waitForFileContent("test_log1.txt");
    waitForFileContent("test_log2.txt");
    std::string logContent1 = readLogFile("test_log1.txt");
    std::string logContent2 = readLogFile("test_log2.txt");

    EXPECT_TRUE(logContent1.find("This message should appear in both logs") != std::string::npos);
    EXPECT_TRUE(logContent2.find("This message should appear in both logs") != std::string::npos);
}

class CustomFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry &entry) const override {
        return "CUSTOM: " + entry.message;
    }
};

TEST_F(LunarLogTest, CustomFormatter) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, CustomFormatter>("custom_formatter_log.txt");

    logger.info("This message should have a custom format");

    waitForFileContent("custom_formatter_log.txt");
    std::string logContent = readLogFile("custom_formatter_log.txt");

    EXPECT_TRUE(logContent.find("CUSTOM: This message should have a custom format") != std::string::npos);
}

TEST_F(LunarLogTest, JsonFormatter) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    waitForFileContent("json_formatter_log.txt");
    std::string logContent = readLogFile("json_formatter_log.txt");

    if (!logContent.empty() && logContent.back() == '\n') {
    logContent.pop_back();
    }

    EXPECT_TRUE(logContent.find(R"("level":"INFO")") != std::string::npos);
    EXPECT_TRUE(logContent.find(R"("message":"User alice logged in from 192.168.1.1")") != std::string::npos);
    EXPECT_TRUE(logContent.find(R"("timestamp":"2024-07-18)") != std::string::npos);  // Check for date part
    EXPECT_TRUE(logContent.find(R"("username":"alice")") != std::string::npos);
    EXPECT_TRUE(logContent.find(R"("ip":"192.168.1.1")") != std::string::npos);

    EXPECT_EQ(logContent.front(), '{');
    EXPECT_EQ(logContent.back(), '}');
    EXPECT_EQ(std::count(logContent.begin(), logContent.end(), '"'), 20);
}

TEST_F(LunarLogTest, XmlFormatter) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    waitForFileContent("xml_formatter_log.txt");
    std::string logContent = readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("<level>INFO</level>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<message>User alice logged in from 192.168.1.1</message>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<timestamp>2024-07-18") != std::string::npos);
    EXPECT_TRUE(logContent.find("<argument name=\"username\">alice") != std::string::npos);
    EXPECT_TRUE(logContent.find("<argument name=\"ip\">192.168.1.1") != std::string::npos);
}