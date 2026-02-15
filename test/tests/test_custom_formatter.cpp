#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class CustomFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry &entry) const override {
        return "CUSTOM: " + entry.message;
    }
};

class CustomFormatterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(CustomFormatterTest, UseCustomFormatter) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, CustomFormatter>("custom_formatter_log.txt");

    logger.info("This message should have a custom format");

    logger.flush();
    TestUtils::waitForFileContent("custom_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("custom_formatter_log.txt");

    EXPECT_TRUE(logContent.find("CUSTOM: This message should have a custom format") != std::string::npos);
}

TEST_F(CustomFormatterTest, MixCustomAndDefaultFormatters) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, CustomFormatter>("custom_formatter_log.txt");
    logger.addSink<minta::FileSink>("default_formatter_log.txt");

    logger.info("This message should appear in both custom and default formats");

    logger.flush();
    TestUtils::waitForFileContent("custom_formatter_log.txt");
    TestUtils::waitForFileContent("default_formatter_log.txt");
    std::string customLogContent = TestUtils::readLogFile("custom_formatter_log.txt");
    std::string defaultLogContent = TestUtils::readLogFile("default_formatter_log.txt");

    EXPECT_TRUE(customLogContent.find("CUSTOM: This message should appear in both custom and default formats") != std::string::npos);
    EXPECT_TRUE(defaultLogContent.find("[INFO] This message should appear in both custom and default formats") != std::string::npos);
}