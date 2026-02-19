#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class LogLevelsTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(LogLevelsTest, RespectLogLevel) {
    minta::LunarLog logger(minta::LogLevel::WARN);
    logger.addSink<minta::FileSink>("level_test_log.txt");

    logger.trace("Trace message");
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warn("Warning message");
    logger.error("Error message");
    logger.fatal("Fatal message");

    logger.flush();
    TestUtils::waitForFileContent("level_test_log.txt");
    std::string logContent = TestUtils::readLogFile("level_test_log.txt");

    EXPECT_TRUE(logContent.find("Trace message") == std::string::npos);
    EXPECT_TRUE(logContent.find("Debug message") == std::string::npos);
    EXPECT_TRUE(logContent.find("Info message") == std::string::npos);
    EXPECT_TRUE(logContent.find("Warning message") != std::string::npos);
    EXPECT_TRUE(logContent.find("Error message") != std::string::npos);
    EXPECT_TRUE(logContent.find("Fatal message") != std::string::npos);
}

TEST_F(LogLevelsTest, ChangeLogLevel) {
    minta::LunarLog logger(minta::LogLevel::ERROR);
    logger.addSink<minta::FileSink>("level_test_log.txt");

    logger.warn("This should not be logged");
    logger.error("This should be logged");

    logger.setMinLevel(minta::LogLevel::WARN);

    logger.warn("This should now be logged");
    logger.info("This should still not be logged");

    logger.flush();
    TestUtils::waitForFileContent("level_test_log.txt");
    std::string logContent = TestUtils::readLogFile("level_test_log.txt");

    EXPECT_TRUE(logContent.find("This should not be logged") == std::string::npos);
    EXPECT_TRUE(logContent.find("This should be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This should now be logged") != std::string::npos);
    EXPECT_TRUE(logContent.find("This should still not be logged") == std::string::npos);
}

TEST_F(LogLevelsTest, GetLevelStringUnknown) {
    auto invalid = static_cast<minta::LogLevel>(99);
    EXPECT_STREQ(minta::getLevelString(invalid), "UNKNOWN");
}