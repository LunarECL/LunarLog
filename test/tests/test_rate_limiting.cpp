#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <chrono>

class RateLimitingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(RateLimitingTest, EnforceRateLimit) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {index}", i);
    }

    logger.flush();
    TestUtils::waitForFileContent("rate_limit_test_log.txt");
    std::string logContent = TestUtils::readLogFile("rate_limit_test_log.txt");

    size_t messageCount = std::count(logContent.begin(), logContent.end(), '\n');
    EXPECT_EQ(messageCount, 1000);
}

TEST_F(RateLimitingTest, ResetAfterRateLimit) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {index}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    TestUtils::waitForFileContent("rate_limit_test_log.txt");
    std::string logContent = TestUtils::readLogFile("rate_limit_test_log.txt");

    EXPECT_TRUE(logContent.find("This message should appear after the rate limit reset") != std::string::npos);
}