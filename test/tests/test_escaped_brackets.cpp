#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class EscapedBracketsTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(EscapedBracketsTest, HandleEscapedBrackets) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("escaped_brackets_test.txt");

    logger.info("This message has escaped brackets: {{escaped}}");
    logger.info("This message has a mix of escaped and unescaped brackets: {{escaped}} and {placeholder}", "value");

    logger.flush();
    TestUtils::waitForFileContent("escaped_brackets_test.txt");
    std::string logContent = TestUtils::readLogFile("escaped_brackets_test.txt");

    EXPECT_TRUE(logContent.find("This message has escaped brackets: {escaped}") != std::string::npos);
    EXPECT_TRUE(logContent.find("This message has a mix of escaped and unescaped brackets: {escaped} and value") != std::string::npos);
}