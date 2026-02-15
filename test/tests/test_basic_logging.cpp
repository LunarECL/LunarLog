#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class BasicLoggingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(BasicLoggingTest, AllLogLevels) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.trace("Trace message");
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warn("Warning message");
    logger.error("Error message");
    logger.fatal("Fatal message");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("[TRACE] Trace message") != std::string::npos);
    EXPECT_TRUE(logContent.find("[DEBUG] Debug message") != std::string::npos);
    EXPECT_TRUE(logContent.find("[INFO] Info message") != std::string::npos);
    EXPECT_TRUE(logContent.find("[WARN] Warning message") != std::string::npos);
    EXPECT_TRUE(logContent.find("[ERROR] Error message") != std::string::npos);
    EXPECT_TRUE(logContent.find("[FATAL] Fatal message") != std::string::npos);
}

TEST_F(BasicLoggingTest, PlaceholderReplacement) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("User {username} logged in from {ip} at {time}", "alice", "192.168.1.1", "14:30");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("User alice logged in from 192.168.1.1 at 14:30") != std::string::npos);
}