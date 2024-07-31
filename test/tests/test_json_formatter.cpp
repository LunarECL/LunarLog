#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "./utils/test_utils.hpp"
#include <regex>
#include <string>

class JsonFormatterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(JsonFormatterTest, ValidJsonOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    // Remove newline at the end if present
    if (!logContent.empty() && logContent.back() == '\n') {
        logContent.pop_back();
    }

    // Basic JSON structure checks
    EXPECT_TRUE(logContent.find('{') != std::string::npos);
    EXPECT_TRUE(logContent.find('}') != std::string::npos);

    // Check for expected fields
    EXPECT_TRUE(logContent.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"timestamp\":") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"User alice logged in from 192.168.1.1\"") != std::string::npos);

    // Validate timestamp format
    std::regex timestampRegex(R"("timestamp":"[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}")");
    EXPECT_TRUE(std::regex_search(logContent, timestampRegex));
}