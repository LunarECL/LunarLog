#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class SuffixFormattingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(SuffixFormattingTest, NoFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Hello {name}", "world");

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Hello world") != std::string::npos);
}

TEST_F(SuffixFormattingTest, FixedPrecision) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Value: {amount:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Value: 3.14") != std::string::npos);
}

TEST_F(SuffixFormattingTest, FixedPrecisionShorthand) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Pi is {pi:4f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Pi is 3.1416") != std::string::npos);
}

TEST_F(SuffixFormattingTest, CurrencyFormat) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Price: {price:C}", 42.5);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Price: $42.50") != std::string::npos);
}

TEST_F(SuffixFormattingTest, CurrencyFormatLowercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Price: {price:c}", 10.0);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Price: $10.00") != std::string::npos);
}

TEST_F(SuffixFormattingTest, HexUppercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Hex: {val:X}", 255);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Hex: FF") != std::string::npos);
}

TEST_F(SuffixFormattingTest, HexLowercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Hex: {val:x}", 255);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Hex: ff") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ScientificNotationLowercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Sci: {val:e}", 12345.6789);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("1.234568e+04") != std::string::npos ||
                logContent.find("1.234568e+004") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ScientificNotationUppercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Sci: {val:E}", 12345.6789);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("1.234568E+04") != std::string::npos ||
                logContent.find("1.234568E+004") != std::string::npos);
}

TEST_F(SuffixFormattingTest, Percentage) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Rate: {rate:P}", 0.856);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Rate: 85.60%") != std::string::npos);
}

TEST_F(SuffixFormattingTest, PercentageLowercase) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Rate: {rate:p}", 0.5);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Rate: 50.00%") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ZeroPadded) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("ID: {id:04}", 42);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("ID: 0042") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NonNumericWithFormat) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Name: {name:.2f}", "alice");

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Name: alice") != std::string::npos);
}

TEST_F(SuffixFormattingTest, MixedFormats) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("{user} spent {amount:C} ({pct:P})", "Bob", 99.99, 0.5);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Bob spent $99.99 (50.00%)") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NegativeCurrency) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Loss: {val:C}", -5.0);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Loss: -$5.00") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NegativeHex) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Neg: {val:X}", -255);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Neg: -FF") != std::string::npos);
}

TEST_F(SuffixFormattingTest, MalformedSpecFallback) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Val: {val:.f}", 3.14);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Val:") != std::string::npos);
}

TEST_F(SuffixFormattingTest, EmptySpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Val: {val:}", 42);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Val: 42") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ZeroPrecision) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("suffix_format_test.txt");

    logger.info("Rounded: {val:.0f}", 3.7);

    logger.flush();
    TestUtils::waitForFileContent("suffix_format_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_format_test.txt");
    EXPECT_TRUE(logContent.find("Rounded: 4") != std::string::npos);
}

TEST_F(SuffixFormattingTest, JsonFormatterAppliesFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("suffix_json_test.txt");

    logger.info("Price: {amount:C}", 42.5);

    logger.flush();
    TestUtils::waitForFileContent("suffix_json_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_json_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"Price: $42.50\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("amount:C") == std::string::npos);
}

TEST_F(SuffixFormattingTest, XmlFormatterAppliesFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("suffix_xml_test.txt");

    logger.info("Rate: {rate:P}", 0.25);

    logger.flush();
    TestUtils::waitForFileContent("suffix_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("suffix_xml_test.txt");

    EXPECT_TRUE(logContent.find("<message>Rate: 25.00%</message>") != std::string::npos);
    EXPECT_TRUE(logContent.find("rate:P") == std::string::npos);
}
