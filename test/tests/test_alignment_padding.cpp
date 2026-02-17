#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>
#include <climits>
#include <locale>

class AlignmentPaddingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// =====================================================================
// Basic Right-Align (positive N)
// =====================================================================

TEST_F(AlignmentPaddingTest, RightAlignBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,20}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[               Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, RightAlignExact) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,5}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

// =====================================================================
// Basic Left-Align (negative N)
// =====================================================================

TEST_F(AlignmentPaddingTest, LeftAlignBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-20}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice               ]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, LeftAlignExact) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-5}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

// =====================================================================
// Zero Alignment (no-op)
// =====================================================================

TEST_F(AlignmentPaddingTest, ZeroAlignmentNoOp) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,0}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, NegativeZeroAlignmentNoOp) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-0}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

// =====================================================================
// No Truncation — value longer than width renders in full
// =====================================================================

TEST_F(AlignmentPaddingTest, NoTruncationRightAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,3}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, NoTruncationLeftAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-3}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

// =====================================================================
// With Format Specifiers (:spec)
// =====================================================================

TEST_F(AlignmentPaddingTest, RightAlignWithFixedPoint) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{price,12:.2f}]", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[        3.14]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, LeftAlignWithFixedPoint) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{price,-12:.2f}]", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[3.14        ]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, AlignWithHexSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{val,8:X}]", 255);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      FF]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, AlignWithCurrencySpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{price,12:C}]", 9.99);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[       $9.99]") != std::string::npos);
}

// =====================================================================
// With Pipe Transforms
// =====================================================================

TEST_F(AlignmentPaddingTest, LeftAlignWithUpper) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-20|upper}]", "alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[ALICE               ]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, RightAlignWithLower) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,10|lower}]", "HELLO");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     hello]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, AlignWithCommaTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{amount,15|comma}]", 1234567);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      1,234,567]") != std::string::npos);
}

// =====================================================================
// With Both Spec and Transforms
// =====================================================================

TEST_F(AlignmentPaddingTest, AlignWithSpecAndTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{amount,18:.2f|comma}]", 1234567.891);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      1,234,567.89]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, AlignWithSpecAndQuote) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{val,10:.2f|quote}]", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    // :.2f -> "3.14", quote -> "\"3.14\"" (6 chars), align 10 -> "    \"3.14\""
    EXPECT_TRUE(content.find("[    \"3.14\"]") != std::string::npos);
}

// =====================================================================
// Indexed Placeholders
// =====================================================================

TEST_F(AlignmentPaddingTest, IndexedRightAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{0,10}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, IndexedLeftAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{0,-10}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice     ]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, IndexedWithSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{1,10:.2f}]", "ignored", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      3.14]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, IndexedReusedWithAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{0,8}] [{0,-8}]", "Hi");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      Hi] [Hi      ]") != std::string::npos);
}

// =====================================================================
// With Operator Prefixes (@, $)
// =====================================================================

TEST_F(AlignmentPaddingTest, DestructureWithAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{@name,10}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, StringifyWithAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{$name,-10}]", "Bob");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Bob       ]") != std::string::npos);
}

// =====================================================================
// Multiple Aligned Placeholders
// =====================================================================

TEST_F(AlignmentPaddingTest, MultipleAlignedPlaceholders) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-10}] [{age,5}]", "Alice", "25");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice     ] [   25]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, MixedAlignedAndUnaligned) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("{greeting} [{name,10}]", "Hello", "World");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("Hello [     World]") != std::string::npos);
}

// =====================================================================
// Empty Value
// =====================================================================

TEST_F(AlignmentPaddingTest, EmptyValueRightAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,5}]", "");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     ]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, EmptyValueLeftAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,-5}]", "");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     ]") != std::string::npos);
}

// =====================================================================
// UTF-8 Value — alignment counts codepoints not bytes
// =====================================================================

TEST_F(AlignmentPaddingTest, Utf8RightAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    // "cafe" with e-acute (2 bytes per char for the acute e)
    std::string cafe = "caf\xc3\xa9";  // 4 codepoints, 5 bytes
    logger.info("[{name,8}]", cafe);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    // 4 codepoints, width 8 -> 4 spaces of padding
    EXPECT_TRUE(content.find("[    caf\xc3\xa9]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, Utf8LeftAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    std::string cafe = "caf\xc3\xa9";  // 4 codepoints, 5 bytes
    logger.info("[{name,-8}]", cafe);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[caf\xc3\xa9    ]") != std::string::npos);
}

// =====================================================================
// Invalid Alignment (fail-open: treated as no alignment)
// =====================================================================

TEST_F(AlignmentPaddingTest, InvalidAlignmentNonNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,abc}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, EmptyAlignmentValue) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,}]", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[Alice]") != std::string::npos);
}

// =====================================================================
// Template Cache — alignment preserved across cached reuse
// =====================================================================

TEST_F(AlignmentPaddingTest, CachedTemplateWithAlignment) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,10}]", "first");
    logger.info("[{name,10}]", "second");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     first]") != std::string::npos);
    EXPECT_TRUE(content.find("[    second]") != std::string::npos);
}

// =====================================================================
// JSON Formatter — alignment in message, raw value in properties
// =====================================================================

TEST_F(AlignmentPaddingTest, JsonMessageAligned) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("align_json.txt");
    logger.info("Name: {name,10}", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_json.txt");
    std::string content = TestUtils::readLogFile("align_json.txt");
    EXPECT_TRUE(content.find("Name:      Alice") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":\"Alice\"") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, JsonDestructureAligned) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("align_json.txt");
    logger.info("Val: {@count,10}", 42);
    logger.flush();
    TestUtils::waitForFileContent("align_json.txt");
    std::string content = TestUtils::readLogFile("align_json.txt");
    EXPECT_TRUE(content.find("Val:         42") != std::string::npos);
    EXPECT_TRUE(content.find("\"count\":42") != std::string::npos);
}

// =====================================================================
// XML Formatter — alignment in message, raw value in properties
// =====================================================================

TEST_F(AlignmentPaddingTest, XmlMessageAligned) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("align_xml.txt");
    logger.info("Name: {name,10}", "Alice");
    logger.flush();
    TestUtils::waitForFileContent("align_xml.txt");
    std::string content = TestUtils::readLogFile("align_xml.txt");
    EXPECT_TRUE(content.find("Name:      Alice") != std::string::npos);
}

// =====================================================================
// Combined with Escaped Braces
// =====================================================================

TEST_F(AlignmentPaddingTest, EscapedBracesWithAlignment) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("{{literal}} [{name,10}]", "test");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("{literal} [      test]") != std::string::npos);
}

// =====================================================================
// No Alignment — existing behavior unchanged
// =====================================================================

TEST_F(AlignmentPaddingTest, NoAlignmentUnchanged) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("Hello {name}", "world");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("Hello world") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, NoAlignmentWithSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("Val: {amount:.2f}", 3.14159);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("Val: 3.14") != std::string::npos);
}

// =====================================================================
// Alignment order: applied AFTER :spec and |transforms
// =====================================================================

TEST_F(AlignmentPaddingTest, OrderSpecThenTransformThenAlign) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    // :x -> "ff", |upper -> "FF", ,8 -> "      FF"
    logger.info("[{v,8:x|upper}]", 255);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      FF]") != std::string::npos);
}

TEST_F(AlignmentPaddingTest, AlignAfterTrimTransform) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    // |trim -> "hello" (5 chars), ,10 -> "     hello"
    logger.info("[{name,10|trim}]", "  hello  ");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[     hello]") != std::string::npos);
}

// =====================================================================
// Unit tests for parseAlignment helper
// =====================================================================

TEST(ParseAlignmentUnit, PositiveNumber) {
    EXPECT_EQ(minta::detail::parseAlignment("20"), 20);
}

TEST(ParseAlignmentUnit, NegativeNumber) {
    EXPECT_EQ(minta::detail::parseAlignment("-20"), -20);
}

TEST(ParseAlignmentUnit, Zero) {
    EXPECT_EQ(minta::detail::parseAlignment("0"), 0);
}

TEST(ParseAlignmentUnit, NegativeZero) {
    EXPECT_EQ(minta::detail::parseAlignment("-0"), 0);
}

TEST(ParseAlignmentUnit, Empty) {
    EXPECT_EQ(minta::detail::parseAlignment(""), 0);
}

TEST(ParseAlignmentUnit, NonNumeric) {
    EXPECT_EQ(minta::detail::parseAlignment("abc"), 0);
}

TEST(ParseAlignmentUnit, MixedChars) {
    EXPECT_EQ(minta::detail::parseAlignment("12abc"), 0);
}

TEST(ParseAlignmentUnit, JustMinus) {
    EXPECT_EQ(minta::detail::parseAlignment("-"), 0);
}

TEST(ParseAlignmentUnit, LargeNumber) {
    EXPECT_EQ(minta::detail::parseAlignment("100"), 100);
}

// =====================================================================
// Unit tests for applyAlignment helper (UTF-8 aware)
// =====================================================================

TEST(ApplyAlignmentUnit, RightAlignAscii) {
    EXPECT_EQ(minta::detail::applyAlignment("hi", 8), "      hi");
}

TEST(ApplyAlignmentUnit, LeftAlignAscii) {
    EXPECT_EQ(minta::detail::applyAlignment("hi", -8), "hi      ");
}

TEST(ApplyAlignmentUnit, NoOpWhenWider) {
    EXPECT_EQ(minta::detail::applyAlignment("hello world", 5), "hello world");
}

TEST(ApplyAlignmentUnit, ZeroAlignment) {
    EXPECT_EQ(minta::detail::applyAlignment("test", 0), "test");
}

TEST(ApplyAlignmentUnit, ExactFit) {
    EXPECT_EQ(minta::detail::applyAlignment("test", 4), "test");
}

TEST(ApplyAlignmentUnit, Utf8Aware) {
    // "cafe" with e-acute: 4 codepoints, 5 bytes
    std::string cafe = "caf\xc3\xa9";
    std::string result = minta::detail::applyAlignment(cafe, 8);
    // 4 codepoints -> 4 spaces of padding, not 3
    EXPECT_EQ(result, "    caf\xc3\xa9");
}

TEST(ApplyAlignmentUnit, EmptyString) {
    EXPECT_EQ(minta::detail::applyAlignment("", 5), "     ");
}

TEST(ApplyAlignmentUnit, EmptyStringLeftAlign) {
    EXPECT_EQ(minta::detail::applyAlignment("", -5), "     ");
}

// =====================================================================
// INT_MIN guard — Finding #1: negating INT_MIN is UB
// =====================================================================

TEST(ApplyAlignmentUnit, IntMinGuard) {
    std::string result = minta::detail::applyAlignment("x", INT_MIN);
    EXPECT_FALSE(result.empty());
    EXPECT_GE(result.size(), 1u);
    EXPECT_NE(result.find('x'), std::string::npos);
}

// =====================================================================
// Alignment width clamp — Finding #2: prevent huge allocations
// =====================================================================

TEST(ParseAlignmentUnit, ClampsLargePositive) {
    int result = minta::detail::parseAlignment("999999999");
    EXPECT_EQ(result, minta::detail::MAX_ALIGNMENT_WIDTH);
}

TEST(ParseAlignmentUnit, ClampsLargeNegative) {
    int result = minta::detail::parseAlignment("-999999999");
    EXPECT_EQ(result, -minta::detail::MAX_ALIGNMENT_WIDTH);
}

TEST(ApplyAlignmentUnit, ClampsExcessiveWidth) {
    std::string result = minta::detail::applyAlignment("x", 2000000);
    EXPECT_LE(result.size(), static_cast<size_t>(minta::detail::MAX_ALIGNMENT_WIDTH) + 1);
}

TEST(ParseAlignmentUnit, AtClampBoundary) {
    int result = minta::detail::parseAlignment("1024");
    EXPECT_EQ(result, 1024);
}

TEST(ParseAlignmentUnit, AboveClampBoundary) {
    int result = minta::detail::parseAlignment("1025");
    EXPECT_EQ(result, 1024);
}

// =====================================================================
// Multi-byte UTF-8 — Finding #6: 3-byte CJK, 4-byte emoji
// =====================================================================

TEST(ApplyAlignmentUnit, Utf8ThreeByteChars) {
    std::string nihongo = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";  // 日本語: 3 codepoints, 9 bytes
    std::string result = minta::detail::applyAlignment(nihongo, 8);
    EXPECT_EQ(result, "     " + nihongo);
}

TEST(ApplyAlignmentUnit, Utf8FourByteEmoji) {
    std::string emoji = "\xf0\x9f\x98\x80";  // 😀: 1 codepoint, 4 bytes
    std::string result = minta::detail::applyAlignment(emoji, 5);
    EXPECT_EQ(result, "    " + emoji);
}

TEST(ApplyAlignmentUnit, Utf8MixedMultiByte) {
    std::string mixed = "A\xc3\xa9\xe6\x97\xa5\xf0\x9f\x98\x80";  // A + é + 日 + 😀 = 4 codepoints
    std::string result = minta::detail::applyAlignment(mixed, 8);
    EXPECT_EQ(result, "    " + mixed);
}

TEST(ApplyAlignmentUnit, Utf8ThreeByteLeftAlign) {
    std::string nihongo = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";  // 日本語: 3 codepoints
    std::string result = minta::detail::applyAlignment(nihongo, -8);
    EXPECT_EQ(result, nihongo + "     ");
}

// =====================================================================
// Explicit {name,0} and {name,-0} no-op — Finding #7
// =====================================================================

TEST(ApplyAlignmentUnit, ZeroIsNoOp) {
    EXPECT_EQ(minta::detail::applyAlignment("hello", 0), "hello");
    EXPECT_EQ(minta::detail::applyAlignment("", 0), "");
}

TEST(ApplyAlignmentUnit, NegativeZeroIsNoOp) {
    EXPECT_EQ(minta::detail::applyAlignment("hello", -0), "hello");
    EXPECT_EQ(minta::detail::applyAlignment("", -0), "");
}

// =====================================================================
// Indexed placeholder + alignment — Finding #8
// =====================================================================

TEST_F(AlignmentPaddingTest, IndexedPlaceholderExplicit) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{0,10}]", "test");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[      test]") != std::string::npos);
}

// =====================================================================
// Alignment + format spec + pipe transform — Finding #9
// =====================================================================

TEST_F(AlignmentPaddingTest, AlignSpecPipeTransformCombined) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{price,15:.2f|comma}]", 1234567.89);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_TRUE(content.find("[   1,234,567.89]") != std::string::npos);
}

// =====================================================================
// Per-sink locale + alignment interaction — Finding #3
// =====================================================================

static bool isLocaleAvailableForAlign(const std::string& name) {
    try {
        std::locale loc(name);
        return true;
    } catch (...) {}
    if (name.find('.') == std::string::npos) {
        try {
            std::locale loc(name + ".UTF-8");
            return true;
        } catch (...) {}
    }
    return false;
}

TEST_F(AlignmentPaddingTest, PerSinkLocalePreservesAlignment) {
    if (!isLocaleAvailableForAlign("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");  // sink 0: C locale

    logger.setSinkLocale(0, "en_US");

    logger.info("Total: [{val,20:n}]", 1234567.89);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");

    EXPECT_NE(content.find("Total: ["), std::string::npos)
        << "Expected aligned locale output. Got: " << content;
    EXPECT_NE(content.find(','), std::string::npos)
        << "en_US locale should produce thousand separators. Got: " << content;
    EXPECT_NE(content.find(']'), std::string::npos);

    size_t openBracket = content.find("Total: [");
    size_t closeBracket = content.find(']', openBracket);
    if (openBracket != std::string::npos && closeBracket != std::string::npos) {
        std::string inner = content.substr(openBracket + 8, closeBracket - openBracket - 8);
        EXPECT_GE(inner.size(), 20u)
            << "Alignment width 20 should be preserved with locale rendering. Inner: '" << inner << "'";
    }
}

TEST_F(AlignmentPaddingTest, PerSinkLocaleDifferentLocalesPreserveAlignment) {
    if (!isLocaleAvailableForAlign("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");  // sink 0

    logger.setLocale("C");
    logger.setSinkLocale(0, "en_US");

    logger.info("AMT:[{amount,18:n}]", 9876543.21);
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");

    size_t marker = content.find("AMT:[");
    ASSERT_NE(marker, std::string::npos) << "Marker not found. Got: " << content;
    size_t openBracket = marker + 4;
    size_t closeBracket = content.find(']', openBracket + 1);
    ASSERT_NE(closeBracket, std::string::npos);
    std::string inner = content.substr(openBracket + 1, closeBracket - openBracket - 1);
    EXPECT_GE(inner.size(), 18u)
        << "Alignment of 18 must be preserved in locale re-render path. Got: '" << inner << "'";
    EXPECT_NE(inner.find(','), std::string::npos)
        << "en_US locale should use comma separators. Got: '" << inner << "'";
}

// =====================================================================
// Integration: huge alignment clamped at template level
// =====================================================================

TEST_F(AlignmentPaddingTest, HugeAlignmentClampedIntegration) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("align_test.txt");
    logger.info("[{name,999999999}]", "hi");
    logger.flush();
    TestUtils::waitForFileContent("align_test.txt");
    std::string content = TestUtils::readLogFile("align_test.txt");
    EXPECT_NE(content.find("hi]"), std::string::npos);
    EXPECT_LE(content.size(), 2048u)
        << "Clamped alignment should prevent huge output. Got size: " << content.size();
}
