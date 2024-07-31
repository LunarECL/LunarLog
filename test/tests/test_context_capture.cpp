#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class ContextCaptureTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(ContextCaptureTest, CaptureGlobalContext) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("context_test_log.txt");

    logger.setCaptureContext(true);
    logger.setContext("session_id", "abc123");
    logger.info("Log with global context");

    TestUtils::waitForFileContent("context_test_log.txt");
    std::string logContent = TestUtils::readLogFile("context_test_log.txt");

    EXPECT_TRUE(logContent.find("session_id=abc123") != std::string::npos);
    EXPECT_TRUE(logContent.find("Log with global context") != std::string::npos);
}

TEST_F(ContextCaptureTest, CaptureScopedContext) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("context_test_log.txt");

    logger.setCaptureContext(true);
    logger.setContext("session_id", "abc123");

    {
        minta::ContextScope scope(logger, "request_id", "req456");
        logger.info("Log within scoped context");
    }

    logger.info("Log after scoped context");

    TestUtils::waitForFileContent("context_test_log.txt");
    std::string logContent = TestUtils::readLogFile("context_test_log.txt");

    EXPECT_TRUE(logContent.find("session_id=abc123") != std::string::npos);
    EXPECT_TRUE(logContent.find("request_id=req456") != std::string::npos);
    EXPECT_TRUE(logContent.find("Log within scoped context") != std::string::npos);
    EXPECT_TRUE(logContent.find("Log after scoped context") != std::string::npos);

    // The second log message should not contain the request_id
    size_t secondLogPos = logContent.find("Log after scoped context");
    EXPECT_TRUE(logContent.find("request_id", secondLogPos) == std::string::npos);
}

TEST_F(ContextCaptureTest, ClearContext) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("context_test_log.txt");

    logger.setCaptureContext(true);
    logger.setContext("session_id", "abc123");
    logger.info("Log with context");

    logger.clearAllContext();
    logger.info("Log after clearing context");

    TestUtils::waitForFileContent("context_test_log.txt");
    std::string logContent = TestUtils::readLogFile("context_test_log.txt");

    EXPECT_TRUE(logContent.find("session_id=abc123") != std::string::npos);
    EXPECT_TRUE(logContent.find("Log with context") != std::string::npos);
    EXPECT_TRUE(logContent.find("Log after clearing context") != std::string::npos);

    // The second log message should not contain the session_id
    size_t secondLogPos = logContent.find("Log after clearing context");
    EXPECT_TRUE(logContent.find("session_id", secondLogPos) == std::string::npos);
}