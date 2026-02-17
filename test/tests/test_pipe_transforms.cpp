#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>

class PipeTransformTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// =====================================================================
// String Transforms
// =====================================================================

TEST_F(PipeTransformTest, Upper) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|upper}", "hello world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: HELLO WORLD") != std::string::npos);
}

TEST_F(PipeTransformTest, Lower) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|lower}", "HELLO WORLD");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello world") != std::string::npos);
}

TEST_F(PipeTransformTest, Trim) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: [{name|trim}]", "  hello  ");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: [hello]") != std::string::npos);
}

TEST_F(PipeTransformTest, TruncateBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|truncate:5}", "hello world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // "hello" + ellipsis (U+2026)
    EXPECT_TRUE(content.find("Val: hello\xe2\x80\xa6") != std::string::npos);
}

TEST_F(PipeTransformTest, TruncateNoOp) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_trunc_noop.txt");
    logger.info("Val: {name|truncate:20}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_trunc_noop.txt");
    std::string content = TestUtils::readLogFile("pipe_trunc_noop.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
    // No ellipsis since string fits
    EXPECT_EQ(content.find("\xe2\x80\xa6"), std::string::npos);
}

TEST_F(PipeTransformTest, TruncateZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: [{name|truncate:0}]", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // truncate:0 on non-empty -> just ellipsis
    EXPECT_TRUE(content.find("Val: [\xe2\x80\xa6]") != std::string::npos);
}

TEST_F(PipeTransformTest, TruncateEmptyString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: [{name|truncate:5}]", "");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: []") != std::string::npos);
}

TEST_F(PipeTransformTest, TruncateUnicode) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    // Each emoji is a multi-byte UTF-8 char; truncate by codepoints not bytes
    std::string input = "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9"; // 5x 'e-acute' (2 bytes each)
    logger.info("Val: {name|truncate:3}", input);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // First 3 codepoints (6 bytes) + ellipsis
    std::string expected = "Val: \xc3\xa9\xc3\xa9\xc3\xa9\xe2\x80\xa6";
    EXPECT_TRUE(content.find(expected) != std::string::npos);
}

TEST_F(PipeTransformTest, PadRight) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("[{name|pad:10}]", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("[hello     ]") != std::string::npos);
}

TEST_F(PipeTransformTest, PadLeft) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("[{name|padl:10}]", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("[     hello]") != std::string::npos);
}

TEST_F(PipeTransformTest, PadZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("[{name|pad:0}]", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("[hello]") != std::string::npos);
}

TEST_F(PipeTransformTest, PadAlreadyLong) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("[{name|pad:3}]", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("[hello]") != std::string::npos);
}

TEST_F(PipeTransformTest, Quote) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|quote}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: \"hello\"") != std::string::npos);
}

// =====================================================================
// Number Transforms
// =====================================================================

TEST_F(PipeTransformTest, CommaInteger) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|comma}", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1,234,567") != std::string::npos);
}

TEST_F(PipeTransformTest, CommaDecimal) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n:.2f|comma}", 1234567.89);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1,234,567.89") != std::string::npos);
}

TEST_F(PipeTransformTest, CommaNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|comma}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

TEST_F(PipeTransformTest, CommaSmallNumber) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|comma}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 42") != std::string::npos);
}

TEST_F(PipeTransformTest, CommaNegative) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|comma}", -1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: -1,234,567") != std::string::npos);
}

TEST_F(PipeTransformTest, HexBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|hex}", 255);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0xff") != std::string::npos);
}

TEST_F(PipeTransformTest, HexZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|hex}", 0);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0x0") != std::string::npos);
}

TEST_F(PipeTransformTest, HexNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|hex}", "abc");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: abc") != std::string::npos);
}

TEST_F(PipeTransformTest, OctBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|oct}", 8);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 010") != std::string::npos);
}

TEST_F(PipeTransformTest, OctZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|oct}", 0);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0") != std::string::npos);
}

TEST_F(PipeTransformTest, BinBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bin}", 10);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0b1010") != std::string::npos);
}

TEST_F(PipeTransformTest, BinZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bin}", 0);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0b0") != std::string::npos);
}

TEST_F(PipeTransformTest, BinNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bin}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

TEST_F(PipeTransformTest, BytesBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bytes}", 1048576);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1.0 MB") != std::string::npos);
}

TEST_F(PipeTransformTest, BytesSmall) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bytes}", 512);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 512 B") != std::string::npos);
}

TEST_F(PipeTransformTest, BytesKB) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bytes}", 1536);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1.5 KB") != std::string::npos);
}

TEST_F(PipeTransformTest, BytesNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|bytes}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

TEST_F(PipeTransformTest, DurationBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|duration}", 3661000);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1h 1m 1s") != std::string::npos);
}

TEST_F(PipeTransformTest, DurationSubSecond) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|duration}", 500);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 500ms") != std::string::npos);
}

TEST_F(PipeTransformTest, DurationZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|duration}", 0);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0s") != std::string::npos);
}

TEST_F(PipeTransformTest, DurationMinutesOnly) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|duration}", 60000);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1m") != std::string::npos);
}

TEST_F(PipeTransformTest, DurationNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|duration}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

TEST_F(PipeTransformTest, PctBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|pct}", 0.856);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 85.6%") != std::string::npos);
}

TEST_F(PipeTransformTest, PctZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|pct}", 0);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 0.0%") != std::string::npos);
}

TEST_F(PipeTransformTest, PctOne) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|pct}", 1);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 100.0%") != std::string::npos);
}

TEST_F(PipeTransformTest, PctNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|pct}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

// =====================================================================
// Structural Transforms
// =====================================================================

TEST_F(PipeTransformTest, ExpandSetsDestructure) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {count|expand}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    // expand sets op='@' so JSON should emit native number
    EXPECT_TRUE(content.find("\"count\":42") != std::string::npos);
}

TEST_F(PipeTransformTest, StrSetsStringify) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {count|str}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    // str sets op='$' so JSON should emit string
    EXPECT_TRUE(content.find("\"count\":\"42\"") != std::string::npos);
}

TEST_F(PipeTransformTest, JsonTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|json}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: \"hello\"") != std::string::npos);
}

TEST_F(PipeTransformTest, JsonTransformNumber) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|json}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 42") != std::string::npos);
}

TEST_F(PipeTransformTest, JsonTransformBool) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {flag|json}", true);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: true") != std::string::npos);
}

TEST_F(PipeTransformTest, JsonTransformNull) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {ptr|json}", nullptr);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: null") != std::string::npos);
}

TEST_F(PipeTransformTest, TypeTransformString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|type}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: string") != std::string::npos);
}

TEST_F(PipeTransformTest, TypeTransformInt) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|type}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: int") != std::string::npos);
}

TEST_F(PipeTransformTest, TypeTransformDouble) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|type}", 3.14);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: double") != std::string::npos);
}

TEST_F(PipeTransformTest, TypeTransformBool) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {flag|type}", true);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: bool") != std::string::npos);
}

TEST_F(PipeTransformTest, TypeTransformNull) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {ptr|type}", nullptr);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: nullptr_t") != std::string::npos);
}

// =====================================================================
// Transform Chaining (2-3 pipes)
// =====================================================================

TEST_F(PipeTransformTest, ChainUpperQuote) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|upper|quote}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: \"HELLO\"") != std::string::npos);
}

TEST_F(PipeTransformTest, ChainTrimUpperTruncate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|trim|upper|truncate:5}", "  hello world  ");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // trim -> "hello world", upper -> "HELLO WORLD", truncate:5 -> "HELLO..."
    EXPECT_TRUE(content.find("Val: HELLO\xe2\x80\xa6") != std::string::npos);
}

TEST_F(PipeTransformTest, ChainCommaQuote) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {n|comma|quote}", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: \"1,234,567\"") != std::string::npos);
}

// =====================================================================
// Combined with Format Specifiers (:spec + pipe)
// =====================================================================

TEST_F(PipeTransformTest, SpecPlusComma) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {amount:.2f|comma}", 1234567.891);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1,234,567.89") != std::string::npos);
}

TEST_F(PipeTransformTest, SpecPlusQuote) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {amount:.2f|quote}", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: \"3.14\"") != std::string::npos);
}

TEST_F(PipeTransformTest, SpecPlusUpper) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {v:x|upper}", 255);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // :x gives "ff", then upper -> "FF"
    EXPECT_TRUE(content.find("Val: FF") != std::string::npos);
}

// =====================================================================
// Combined with Operators (@ + pipe, $ + pipe)
// =====================================================================

TEST_F(PipeTransformTest, DestructurePlusComma) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {@amount|comma}", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 1,234,567") != std::string::npos);
}

TEST_F(PipeTransformTest, DestructurePlusCommaJson) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {@amount|comma}", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    // Message has comma-formatted value
    EXPECT_TRUE(content.find("Val: 1,234,567") != std::string::npos);
    // Property has raw value with native type (@ operator)
    EXPECT_TRUE(content.find("\"amount\":1234567") != std::string::npos);
}

TEST_F(PipeTransformTest, StringifyPlusUpper) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {$name|upper}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: HELLO") != std::string::npos);
}

TEST_F(PipeTransformTest, OperatorPlusSpecPlusPipe) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    // Full combo: operator + spec + pipe
    logger.info("Val: {@amount:.2f|comma|quote}", 1234567.891);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // :2f -> "1234567.89", comma -> "1,234,567.89", quote -> "\"1,234,567.89\""
    EXPECT_TRUE(content.find("Val: \"1,234,567.89\"") != std::string::npos);
}

// =====================================================================
// Edge Cases
// =====================================================================

TEST_F(PipeTransformTest, EmptyValueString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: [{name|upper}]", "");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: []") != std::string::npos);
}

TEST_F(PipeTransformTest, UnknownTransformPassThrough) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|nonexistent}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello") != std::string::npos);
}

TEST_F(PipeTransformTest, UnknownTransformInChain) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|upper|nonexistent|quote}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // upper -> "HELLO", nonexistent -> "HELLO" (pass through), quote -> "\"HELLO\""
    EXPECT_TRUE(content.find("Val: \"HELLO\"") != std::string::npos);
}

TEST_F(PipeTransformTest, NoArgForParameterizedTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    // truncate without arg -> no-op (pipeSafeStoi("", -1) returns -1 -> no truncation)
    logger.info("Val: {name|truncate}", "hello world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: hello world") != std::string::npos);
}

TEST_F(PipeTransformTest, NegativeArgForTruncate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {name|truncate:-5}", "hello world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    // Negative arg -> no-op
    EXPECT_TRUE(content.find("Val: hello world") != std::string::npos);
}

TEST_F(PipeTransformTest, NoPipeWorksAsBeforePlain) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Hello {name}", "world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Hello world") != std::string::npos);
}

TEST_F(PipeTransformTest, NoPipeWithSpecWorksAsBefore) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {amount:.2f}", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 3.14") != std::string::npos);
    EXPECT_EQ(content.find("Val: 3.14159"), std::string::npos);
}

TEST_F(PipeTransformTest, NoPipeOperatorsWorkAsBefore) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("Val: {@id}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: 42") != std::string::npos);
}

TEST_F(PipeTransformTest, EscapedBracesStillWork) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.info("{{literal}} {name|upper}", "test");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("{literal} TEST") != std::string::npos);
}

// =====================================================================
// JSON Formatter: transforms metadata
// =====================================================================

TEST_F(PipeTransformTest, JsonIncludesTransforms) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {name|upper|quote}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    EXPECT_TRUE(content.find("\"transforms\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":[\"upper\",\"quote\"]") != std::string::npos);
}

TEST_F(PipeTransformTest, JsonNoTransformsFieldWhenNoPipes) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json_noxf.txt");
    logger.info("Val: {name}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_json_noxf.txt");
    std::string content = TestUtils::readLogFile("pipe_json_noxf.txt");
    EXPECT_EQ(content.find("\"transforms\""), std::string::npos);
}

TEST_F(PipeTransformTest, JsonTransformsWithArgs) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {name|truncate:5}", "hello world");
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    EXPECT_TRUE(content.find("\"transforms\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":[\"truncate:5\"]") != std::string::npos);
}

// =====================================================================
// XML Formatter: transforms attribute
// =====================================================================

TEST_F(PipeTransformTest, XmlIncludesTransforms) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("pipe_xml.txt");
    logger.info("Val: {name|upper|quote}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_xml.txt");
    std::string content = TestUtils::readLogFile("pipe_xml.txt");
    EXPECT_TRUE(content.find("transforms=\"upper|quote\"") != std::string::npos);
}

TEST_F(PipeTransformTest, XmlNoTransformsWhenNoPipes) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("pipe_xml_noxf.txt");
    logger.info("Val: {name}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_xml_noxf.txt");
    std::string content = TestUtils::readLogFile("pipe_xml_noxf.txt");
    EXPECT_EQ(content.find("transforms="), std::string::npos);
}

TEST_F(PipeTransformTest, XmlDestructurePlusTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("pipe_xml.txt");
    logger.info("Val: {@name|upper}", "hello");
    logger.flush();
    TestUtils::waitForFileContent("pipe_xml.txt");
    std::string content = TestUtils::readLogFile("pipe_xml.txt");
    EXPECT_TRUE(content.find("destructure=\"true\"") != std::string::npos);
    EXPECT_TRUE(content.find("transforms=\"upper\"") != std::string::npos);
}

// =====================================================================
// Template Cache: cached transforms
// =====================================================================

TEST_F(PipeTransformTest, CachedTemplateWithTransforms) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    // Log same template twice - second should use cache
    logger.info("Val: {name|upper}", "first");
    logger.info("Val: {name|upper}", "second");
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    std::string content = TestUtils::readLogFile("pipe_test.txt");
    EXPECT_TRUE(content.find("Val: FIRST") != std::string::npos);
    EXPECT_TRUE(content.find("Val: SECOND") != std::string::npos);
}

// =====================================================================
// Expand/Str structural transform overrides operator
// =====================================================================

TEST_F(PipeTransformTest, ExpandOverridesNoOp) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    // No operator prefix, but expand transform should set op='@'
    logger.info("Val: {count|expand}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    EXPECT_TRUE(content.find("\"count\":42") != std::string::npos);
}

TEST_F(PipeTransformTest, StrOverridesDestructure) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    // @ prefix, but str transform should override to '$'
    logger.info("Val: {@count|str}", 42);
    logger.flush();
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string content = TestUtils::readLogFile("pipe_json.txt");
    EXPECT_TRUE(content.find("\"count\":\"42\"") != std::string::npos);
}

// =====================================================================
// Expand + string transform combo
// =====================================================================

TEST_F(PipeTransformTest, ExpandPlusCommaInMessage) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pipe_test.txt");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("pipe_json.txt");
    logger.info("Val: {amount|expand|comma}", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("pipe_test.txt");
    TestUtils::waitForFileContent("pipe_json.txt");
    std::string humanContent = TestUtils::readLogFile("pipe_test.txt");
    std::string jsonContent = TestUtils::readLogFile("pipe_json.txt");
    // Human-readable: comma applied
    EXPECT_TRUE(humanContent.find("Val: 1,234,567") != std::string::npos);
    // JSON: raw value with native type (expand set @)
    EXPECT_TRUE(jsonContent.find("\"amount\":1234567") != std::string::npos);
}
