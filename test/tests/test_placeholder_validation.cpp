#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class PlaceholderValidationTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(PlaceholderValidationTest, EmptyPlaceholder) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("validation_test_log.txt");

    logger.info("Empty placeholder: {}", "value");

    TestUtils::waitForFileContent("validation_test_log.txt");
    std::string logContent = TestUtils::readLogFile("validation_test_log.txt");

    EXPECT_TRUE(logContent.find("Warning: Empty placeholder found") != std::string::npos);
    EXPECT_TRUE(logContent.find("Empty placeholder: value") != std::string::npos);
}

TEST_F(PlaceholderValidationTest, RepeatedPlaceholder) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("validation_test_log.txt");

    logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");

    TestUtils::waitForFileContent("validation_test_log.txt");
    std::string logContent = TestUtils::readLogFile("validation_test_log.txt");

    EXPECT_TRUE(logContent.find("Warning: Repeated placeholder name: placeholder") != std::string::npos);
    EXPECT_TRUE(logContent.find("Repeated placeholder: value1 and value2") != std::string::npos);
}

TEST_F(PlaceholderValidationTest, TooFewValues) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("validation_test_log.txt");

    logger.info("Too few values: {placeholder1} and {placeholder2}", "value");

    TestUtils::waitForFileContent("validation_test_log.txt");
    std::string logContent = TestUtils::readLogFile("validation_test_log.txt");

    EXPECT_TRUE(logContent.find("Warning: More placeholders than provided values") != std::string::npos);
    EXPECT_TRUE(logContent.find("Too few values: value and {placeholder2}") != std::string::npos);
}

TEST_F(PlaceholderValidationTest, TooManyValues) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("validation_test_log.txt");

    logger.info("Too many values: {placeholder}", "value1", "value2");

    TestUtils::waitForFileContent("validation_test_log.txt");
    std::string logContent = TestUtils::readLogFile("validation_test_log.txt");

    EXPECT_TRUE(logContent.find("Warning: More values provided than placeholders") != std::string::npos);
    EXPECT_TRUE(logContent.find("Too many values: value1") != std::string::npos);
}