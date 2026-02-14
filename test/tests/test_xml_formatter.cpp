#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <regex>
#include <string>

class XmlFormatterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(XmlFormatterTest, ValidXmlOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    // Basic XML structure checks
    EXPECT_TRUE(logContent.find("<log_entry>") != std::string::npos);
    EXPECT_TRUE(logContent.find("</log_entry>") != std::string::npos);

    // Check for expected fields
    EXPECT_TRUE(logContent.find("<level>INFO</level>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<timestamp>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<message>User alice logged in from 192.168.1.1</message>") != std::string::npos);

    // Validate timestamp format
    std::regex timestampRegex(R"(<timestamp>[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}</timestamp>)");
    EXPECT_TRUE(std::regex_search(logContent, timestampRegex));
}