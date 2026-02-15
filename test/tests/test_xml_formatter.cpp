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

    logger.flush();
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

TEST_F(XmlFormatterTest, SanitizeXmlNameEmpty) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.setCaptureSourceLocation(true);
    logger.setContext("", "empty_key_val");
    logger.info("test empty key");

    logger.flush();
    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("<_>empty_key_val</_>") != std::string::npos);
}

TEST_F(XmlFormatterTest, SanitizeXmlNameDigitLeading) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.setContext("123abc", "digit_val");
    logger.info("test digit key");

    logger.flush();
    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("<_23abc>digit_val</_23abc>") != std::string::npos);
}

TEST_F(XmlFormatterTest, SanitizeXmlNameSpaces) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.setContext("my key", "space_val");
    logger.info("test space key");

    logger.flush();
    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("<my_key>space_val</my_key>") != std::string::npos);
}

TEST_F(XmlFormatterTest, EscapeXmlStringSpecialChars) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    logger.info("chars {v}", "<b>bold</b> & \"quotes\"");

    logger.flush();
    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("&lt;b&gt;bold&lt;/b&gt; &amp; &quot;quotes&quot;") != std::string::npos);
}

TEST_F(XmlFormatterTest, EscapeXmlStringControlChars) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml_formatter_log.txt");

    std::string ctrlStr = "before";
    ctrlStr += '\x01';
    ctrlStr += "after";
    logger.info("ctrl {v}", ctrlStr);

    logger.flush();
    TestUtils::waitForFileContent("xml_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("xml_formatter_log.txt");

    EXPECT_TRUE(logContent.find("before after") != std::string::npos);
    EXPECT_TRUE(logContent.find('\x01') == std::string::npos);
}