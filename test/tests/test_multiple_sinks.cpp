#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "./utils/test_utils.hpp"

class MultipleSinksTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(MultipleSinksTest, LogToMultipleSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("test_log1.txt");
    logger.addSink<minta::FileSink>("test_log2.txt");

    logger.info("This message should appear in both logs");

    TestUtils::waitForFileContent("test_log1.txt");
    TestUtils::waitForFileContent("test_log2.txt");
    std::string logContent1 = TestUtils::readLogFile("test_log1.txt");
    std::string logContent2 = TestUtils::readLogFile("test_log2.txt");

    EXPECT_TRUE(logContent1.find("This message should appear in both logs") != std::string::npos);
    EXPECT_TRUE(logContent2.find("This message should appear in both logs") != std::string::npos);
}

TEST_F(MultipleSinksTest, DifferentFormattersForDifferentSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("test_log1.txt");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("test_log2.json");

    logger.info("Test message for multiple formatters");

    TestUtils::waitForFileContent("test_log1.txt");
    TestUtils::waitForFileContent("test_log2.json");
    std::string logContent1 = TestUtils::readLogFile("test_log1.txt");
    std::string logContent2 = TestUtils::readLogFile("test_log2.json");

    EXPECT_TRUE(logContent1.find("Test message for multiple formatters") != std::string::npos);
    EXPECT_TRUE(logContent2.find("\"message\":\"Test message for multiple formatters\"") != std::string::npos);
}