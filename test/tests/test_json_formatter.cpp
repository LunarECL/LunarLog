#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <regex>
#include <string>
#include <cmath>
#include <limits>

class JsonFormatterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(JsonFormatterTest, ValidJsonOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    logger.flush();
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

TEST_F(JsonFormatterTest, EscapeJsonStringQuotes) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("He said {v}", "\"hello\"");

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(logContent.find(R"(He said \"hello\")") != std::string::npos);
}

TEST_F(JsonFormatterTest, EscapeJsonStringBackslash) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("path {v}", "C:\\Users\\test");

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(logContent.find(R"(C:\\Users\\test)") != std::string::npos);
}

TEST_F(JsonFormatterTest, EscapeJsonStringNewlineAndTab) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("lines {v}", "a\nb\tc");

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(logContent.find(R"(a\nb\tc)") != std::string::npos);
}

TEST_F(JsonFormatterTest, EscapeJsonStringControlChar) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    std::string ctrlStr(1, '\x01');
    logger.info("ctrl {v}", ctrlStr);

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(logContent.find("\\u0001") != std::string::npos);
}

TEST_F(JsonFormatterTest, EscapeJsonStringUnicode) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("emoji {v}", "\xC3\xA9");

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string logContent = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(logContent.find("\xC3\xA9") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Destructure edge cases (shared json_detail via JsonFormatter)
// ---------------------------------------------------------------------------

TEST_F(JsonFormatterTest, DestructureEmptyString) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("Val: {@v}", "");

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string content = TestUtils::readLogFile("json_formatter_log.txt");

    EXPECT_TRUE(content.find("\"v\":\"\"") != std::string::npos);
}

TEST_F(JsonFormatterTest, DestructureNaN) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("Val: {@v}", std::numeric_limits<double>::quiet_NaN());

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string content = TestUtils::readLogFile("json_formatter_log.txt");

    // NaN is not a valid JSON number; must be emitted as a string
    EXPECT_TRUE(content.find("\"v\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"v\":nan") == std::string::npos);
}

TEST_F(JsonFormatterTest, DestructureInfinity) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("Pos: {@pos}, Neg: {@neg}",
                std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity());

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string content = TestUtils::readLogFile("json_formatter_log.txt");

    // Infinity is not a valid JSON number; must be emitted as a string
    EXPECT_TRUE(content.find("\"pos\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"neg\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"pos\":inf") == std::string::npos);
    EXPECT_TRUE(content.find("\"neg\":-inf") == std::string::npos);
}

TEST_F(JsonFormatterTest, DestructureNegativeZero) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json_formatter_log.txt");

    logger.info("Val: {@v}", -0.0);

    logger.flush();
    TestUtils::waitForFileContent("json_formatter_log.txt");
    std::string content = TestUtils::readLogFile("json_formatter_log.txt");

    // Negative zero should normalize to 0 in JSON output
    EXPECT_TRUE(content.find("\"v\":0") != std::string::npos);
    EXPECT_TRUE(content.find("\"v\":-0") == std::string::npos);
}

TEST_F(JsonFormatterTest, LocaleIndependentNumericParse) {
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("3.14159"), "3.14159");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("42"), "42");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("-7"), "-7");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("true"), "true");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("false"), "false");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue(""), "\"\"");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("hello"), "\"hello\"");
}