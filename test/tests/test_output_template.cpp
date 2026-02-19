#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <regex>
#include <thread>
#include <sstream>

class OutputTemplateTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ---------------------------------------------------------------------------
// Parser unit tests (detail::parseOutputTemplate + detail::OutputTemplate)
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ParseLiteralOnly) {
    auto segments = minta::detail::parseOutputTemplate("Hello World");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "Hello World");
}

TEST_F(OutputTemplateTest, ParseMessageToken) {
    auto segments = minta::detail::parseOutputTemplate("{message}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_FALSE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Message);
}

TEST_F(OutputTemplateTest, ParseTimestampWithSpec) {
    auto segments = minta::detail::parseOutputTemplate("{timestamp:HH:mm:ss}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_FALSE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Timestamp);
    EXPECT_FALSE(segments[0].spec.empty());
}

TEST_F(OutputTemplateTest, ParseLevelU3) {
    auto segments = minta::detail::parseOutputTemplate("{level:u3}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Level);
    EXPECT_EQ(segments[0].spec, "u3");
}

TEST_F(OutputTemplateTest, ParseLevelLower) {
    auto segments = minta::detail::parseOutputTemplate("{level:l}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].spec, "l");
}

TEST_F(OutputTemplateTest, ParseAlignmentRightAlign) {
    auto segments = minta::detail::parseOutputTemplate("{level,8}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, 8);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Level);
}

TEST_F(OutputTemplateTest, ParseAlignmentLeftAlign) {
    auto segments = minta::detail::parseOutputTemplate("{level,-8}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, -8);
}

TEST_F(OutputTemplateTest, ParseAlignmentWithSpec) {
    auto segments = minta::detail::parseOutputTemplate("{level,10:u3}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, 10);
    EXPECT_EQ(segments[0].spec, "u3");
}

TEST_F(OutputTemplateTest, ParseEscapedBraces) {
    auto segments = minta::detail::parseOutputTemplate("{{literal}} text");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "{literal} text");
}

TEST_F(OutputTemplateTest, ParseUnknownTokenProducesEmpty) {
    auto segments = minta::detail::parseOutputTemplate("before{unknown}after");
    ASSERT_EQ(segments.size(), 3u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "before");
    EXPECT_TRUE(segments[1].isLiteral);
    EXPECT_EQ(segments[1].literal, "");
    EXPECT_TRUE(segments[2].isLiteral);
    EXPECT_EQ(segments[2].literal, "after");
}

TEST_F(OutputTemplateTest, ParseAllTokenTypes) {
    std::vector<std::string> tokens = {
        "{timestamp}", "{level}", "{message}", "{newline}",
        "{properties}", "{template}", "{source}", "{threadId}", "{exception}"
    };
    for (const auto& tok : tokens) {
        auto segments = minta::detail::parseOutputTemplate(tok);
        ASSERT_EQ(segments.size(), 1u) << "Token: " << tok;
        EXPECT_FALSE(segments[0].isLiteral) << "Token: " << tok;
    }
}

TEST_F(OutputTemplateTest, ParseCompositeTemplate) {
    auto segments = minta::detail::parseOutputTemplate("{timestamp:HH:mm:ss} [{level:u3}] {message}");
    ASSERT_EQ(segments.size(), 5u);
    EXPECT_FALSE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Timestamp);
    EXPECT_TRUE(segments[1].isLiteral);
    EXPECT_EQ(segments[1].literal, " [");
    EXPECT_FALSE(segments[2].isLiteral);
    EXPECT_EQ(segments[2].tokenType, minta::detail::OutputTokenType::Level);
    EXPECT_EQ(segments[2].spec, "u3");
    EXPECT_TRUE(segments[3].isLiteral);
    EXPECT_EQ(segments[3].literal, "] ");
    EXPECT_FALSE(segments[4].isLiteral);
    EXPECT_EQ(segments[4].tokenType, minta::detail::OutputTokenType::Message);
}

TEST_F(OutputTemplateTest, EmptyTemplate) {
    auto segments = minta::detail::parseOutputTemplate("");
    EXPECT_TRUE(segments.empty());
}

// ---------------------------------------------------------------------------
// Timestamp format conversion
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ConvertTimestampFormat) {
    std::string converted = minta::detail::convertTimestampFormat("yyyy-MM-dd HH:mm:ss.fff");
    EXPECT_EQ(converted, std::string("%Y-%m-%d %H:%M:%S.") + '\x01');
}

TEST_F(OutputTemplateTest, ConvertTimestampFormatPartial) {
    EXPECT_EQ(minta::detail::convertTimestampFormat("HH:mm:ss"), "%H:%M:%S");
    EXPECT_EQ(minta::detail::convertTimestampFormat("yyyy"), "%Y");
}

// ---------------------------------------------------------------------------
// Level rendering
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, LevelU3AllLevels) {
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::TRACE), "TRC");
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::DEBUG), "DBG");
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::INFO),  "INF");
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::WARN),  "WRN");
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::ERROR), "ERR");
    EXPECT_STREQ(minta::detail::getLevelU3(minta::LogLevel::FATAL), "FTL");
}

TEST_F(OutputTemplateTest, LevelLowerAllLevels) {
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::TRACE), "trace");
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::DEBUG), "debug");
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::INFO),  "info");
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::WARN),  "warn");
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::ERROR), "error");
    EXPECT_STREQ(minta::detail::getLevelLower(minta::LogLevel::FATAL), "fatal");
}

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, AlignmentRight) {
    EXPECT_EQ(minta::detail::applyAlignment("INF", 8), "     INF");
}

TEST_F(OutputTemplateTest, AlignmentLeft) {
    EXPECT_EQ(minta::detail::applyAlignment("INF", -8), "INF     ");
}

TEST_F(OutputTemplateTest, AlignmentNoOpWhenWider) {
    EXPECT_EQ(minta::detail::applyAlignment("INFORMATION", 5), "INFORMATION");
}

TEST_F(OutputTemplateTest, AlignmentZero) {
    EXPECT_EQ(minta::detail::applyAlignment("test", 0), "test");
}

// ---------------------------------------------------------------------------
// Full render tests using OutputTemplate directly
// ---------------------------------------------------------------------------

// Timestamp render tests use regex patterns (not exact values) so they pass
// regardless of the CI machine's timezone. Do not add exact-value timestamp
// assertions here — use regex or non-empty checks instead.
static minta::LogEntry makeTestEntry(
    minta::LogLevel level,
    const std::string& message,
    const std::string& templateStr = "",
    const std::string& file = "",
    int line = 0,
    const std::string& function = "",
    const std::map<std::string, std::string>& context = {},
    std::thread::id tid = std::this_thread::get_id()
) {
    // Fixed time point for deterministic tests: 2024-02-17 12:00:00 UTC
    auto tp = std::chrono::system_clock::from_time_t(1708185600);
    return minta::LogEntry(
        level, message,
        tp,
        templateStr.empty() ? message : templateStr,
        0, {}, file, line, function, context, {},
        {}, "C", tid
    );
}

TEST_F(OutputTemplateTest, RenderMessageOnly) {
    minta::detail::OutputTemplate tpl("{message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "Hello World");
    EXPECT_EQ(tpl.render(entry), "Hello World");
}

TEST_F(OutputTemplateTest, RenderLevelDefault) {
    minta::detail::OutputTemplate tpl("{level}");
    auto entry = makeTestEntry(minta::LogLevel::WARN, "msg");
    EXPECT_EQ(tpl.render(entry), "WARN");
}

TEST_F(OutputTemplateTest, RenderLevelU3) {
    minta::detail::OutputTemplate tpl("{level:u3}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "INF");
}

TEST_F(OutputTemplateTest, RenderLevelLowercase) {
    minta::detail::OutputTemplate tpl("{level:l}");
    auto entry = makeTestEntry(minta::LogLevel::ERROR, "msg");
    EXPECT_EQ(tpl.render(entry), "error");
}

TEST_F(OutputTemplateTest, RenderNewline) {
    minta::detail::OutputTemplate tpl("line1{newline}line2");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "line1\nline2");
}

TEST_F(OutputTemplateTest, RenderTemplate) {
    minta::detail::OutputTemplate tpl("tpl={template}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "User alice logged in", "User {name} logged in");
    EXPECT_EQ(tpl.render(entry), "tpl=User {name} logged in");
}

TEST_F(OutputTemplateTest, RenderSource) {
    minta::detail::OutputTemplate tpl("{source}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg", "", "main.cpp", 42, "main");
    EXPECT_EQ(tpl.render(entry), "main.cpp:42 main");
}

TEST_F(OutputTemplateTest, RenderSourceEmpty) {
    minta::detail::OutputTemplate tpl("{source}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "");
}

TEST_F(OutputTemplateTest, RenderThreadId) {
    minta::detail::OutputTemplate tpl("{threadId}");
    std::thread::id thisId = std::this_thread::get_id();
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg", "", "", 0, "", {}, thisId);
    std::string result = tpl.render(entry);
    EXPECT_FALSE(result.empty());
    std::ostringstream oss;
    oss << thisId;
    EXPECT_EQ(result, oss.str());
}

TEST_F(OutputTemplateTest, RenderProperties) {
    std::map<std::string, std::string> ctx;
    ctx["env"] = "prod";
    ctx["region"] = "us-east";
    minta::detail::OutputTemplate tpl("{properties}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg", "", "", 0, "", ctx);
    std::string result = tpl.render(entry);
    EXPECT_TRUE(result.find("env=prod") != std::string::npos);
    EXPECT_TRUE(result.find("region=us-east") != std::string::npos);
}

TEST_F(OutputTemplateTest, RenderPropertiesEmpty) {
    minta::detail::OutputTemplate tpl("{properties}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "");
}

TEST_F(OutputTemplateTest, RenderException) {
    minta::detail::OutputTemplate tpl("{exception}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "");
}

TEST_F(OutputTemplateTest, RenderEscapedBraces) {
    minta::detail::OutputTemplate tpl("{{literal}} {message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "hello");
    EXPECT_EQ(tpl.render(entry), "{literal} hello");
}

TEST_F(OutputTemplateTest, RenderDoubleCloseBrace) {
    minta::detail::OutputTemplate tpl("{message} }}end");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "hello");
    EXPECT_EQ(tpl.render(entry), "hello }end");
}

TEST_F(OutputTemplateTest, RenderUnknownTokenEmpty) {
    minta::detail::OutputTemplate tpl("before{bogus}after");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "beforeafter");
}

TEST_F(OutputTemplateTest, RenderAlignmentInTemplate) {
    minta::detail::OutputTemplate tpl("[{level,8}]");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "[    INFO]");
}

TEST_F(OutputTemplateTest, RenderLeftAlignmentInTemplate) {
    minta::detail::OutputTemplate tpl("[{level,-8}]");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "[INFO    ]");
}

TEST_F(OutputTemplateTest, RenderAlignmentWithU3) {
    minta::detail::OutputTemplate tpl("[{level,6:u3}]");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "[   INF]");
}

TEST_F(OutputTemplateTest, RenderTimestampDefault) {
    minta::detail::OutputTemplate tpl("{timestamp}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    std::string result = tpl.render(entry);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find(":") != std::string::npos);
}

TEST_F(OutputTemplateTest, RenderTimestampCustomFormat) {
    minta::detail::OutputTemplate tpl("{timestamp:HH:mm:ss}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    std::string result = tpl.render(entry);
    std::regex timePattern("\\d{2}:\\d{2}:\\d{2}");
    EXPECT_TRUE(std::regex_match(result, timePattern)) << "Got: " << result;
}

TEST_F(OutputTemplateTest, RenderTimestampWithMillis) {
    minta::detail::OutputTemplate tpl("{timestamp:HH:mm:ss.fff}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    std::string result = tpl.render(entry);
    std::regex timePattern("\\d{2}:\\d{2}:\\d{2}\\.\\d{3}");
    EXPECT_TRUE(std::regex_match(result, timePattern)) << "Got: " << result;
}

TEST_F(OutputTemplateTest, RenderTimestampFullDate) {
    minta::detail::OutputTemplate tpl("{timestamp:yyyy-MM-dd HH:mm:ss}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    std::string result = tpl.render(entry);
    std::regex dtPattern("\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}");
    EXPECT_TRUE(std::regex_match(result, dtPattern)) << "Got: " << result;
}

TEST_F(OutputTemplateTest, RenderCompositeTemplate) {
    minta::detail::OutputTemplate tpl("{timestamp:HH:mm:ss} [{level:u3}] {message}");
    auto entry = makeTestEntry(minta::LogLevel::WARN, "disk full");
    std::string result = tpl.render(entry);
    std::regex pattern("\\d{2}:\\d{2}:\\d{2} \\[WRN\\] disk full");
    EXPECT_TRUE(std::regex_match(result, pattern)) << "Got: " << result;
}

// ---------------------------------------------------------------------------
// HumanReadableFormatter integration
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, FormatterDefaultBehaviorUnchanged) {
    minta::HumanReadableFormatter fmt;
    auto entry = makeTestEntry(minta::LogLevel::INFO, "test message");
    std::string result = fmt.format(entry);
    EXPECT_TRUE(result.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(result.find("test message") != std::string::npos);
}

TEST_F(OutputTemplateTest, FormatterWithOutputTemplate) {
    minta::HumanReadableFormatter fmt;
    fmt.setOutputTemplate("[{level:u3}] {message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "hello");
    EXPECT_EQ(fmt.format(entry), "[INF] hello");
}

TEST_F(OutputTemplateTest, FormatterClearOutputTemplate) {
    minta::HumanReadableFormatter fmt;
    fmt.setOutputTemplate("[{level:u3}] {message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "test");
    std::string withTemplate = fmt.format(entry);
    EXPECT_EQ(withTemplate, "[INF] test");

    fmt.setOutputTemplate("");
    std::string withoutTemplate = fmt.format(entry);
    EXPECT_TRUE(withoutTemplate.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(withoutTemplate.find("test") != std::string::npos);
}

// ---------------------------------------------------------------------------
// SinkProxy integration (full logger pipeline)
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, SinkProxyOutputTemplate) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>(minta::named("out"), "test_output_tpl.txt");
    logger.sink("out").outputTemplate("[{level:u3}] {message}");
    logger.info("Hello output template");
    logger.flush();
    TestUtils::waitForFileContent("test_output_tpl.txt");
    std::string content = TestUtils::readLogFile("test_output_tpl.txt");
    EXPECT_TRUE(content.find("[INF] Hello output template") != std::string::npos)
        << "Got: " << content;
}

TEST_F(OutputTemplateTest, SinkProxyOutputTemplateDoesNotAffectOtherSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>(minta::named("templated"), "test_otpl_a.txt");
    logger.addSink<minta::FileSink>(minta::named("default"), "test_otpl_b.txt");
    logger.sink("templated").outputTemplate("[{level:u3}] {message}");
    logger.info("Check isolation");
    logger.flush();
    TestUtils::waitForFileContent("test_otpl_a.txt");
    TestUtils::waitForFileContent("test_otpl_b.txt");
    std::string a = TestUtils::readLogFile("test_otpl_a.txt");
    std::string b = TestUtils::readLogFile("test_otpl_b.txt");
    EXPECT_TRUE(a.find("[INF] Check isolation") != std::string::npos) << "a: " << a;
    EXPECT_TRUE(b.find("[INFO]") != std::string::npos) << "b: " << b;
}

TEST_F(OutputTemplateTest, SinkProxyOnNonHumanFormatterIsNoOp) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>(minta::named("json"), "test_otpl_json.txt");
    logger.sink("json").outputTemplate("[{level:u3}] {message}");
    logger.info("JSON test");
    logger.flush();
    TestUtils::waitForFileContent("test_otpl_json.txt");
    std::string content = TestUtils::readLogFile("test_otpl_json.txt");
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos) << "Got: " << content;
}

TEST_F(OutputTemplateTest, EmptyTemplateProducesEmptyFromParser) {
    minta::detail::OutputTemplate tpl("");
    EXPECT_TRUE(tpl.empty());
}

TEST_F(OutputTemplateTest, MultipleUnknownTokensAllEmpty) {
    minta::detail::OutputTemplate tpl("{foo}{bar}{baz}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "");
}

TEST_F(OutputTemplateTest, MixedKnownAndUnknownTokens) {
    minta::detail::OutputTemplate tpl("{level}-{unknownThing}-{message}");
    auto entry = makeTestEntry(minta::LogLevel::DEBUG, "hi");
    EXPECT_EQ(tpl.render(entry), "DEBUG--hi");
}

TEST_F(OutputTemplateTest, OnlyEscapedBraces) {
    minta::detail::OutputTemplate tpl("{{}}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    EXPECT_EQ(tpl.render(entry), "{}");
}

TEST_F(OutputTemplateTest, ConsecutiveTokensNoSeparator) {
    minta::detail::OutputTemplate tpl("{level}{message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "test");
    EXPECT_EQ(tpl.render(entry), "INFOtest");
}

TEST_F(OutputTemplateTest, TimestampYearOnly) {
    minta::detail::OutputTemplate tpl("{timestamp:yyyy}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg");
    std::string result = tpl.render(entry);
    std::regex yearPattern("\\d{4}");
    EXPECT_TRUE(std::regex_match(result, yearPattern)) << "Got: " << result;
}

TEST_F(OutputTemplateTest, ThreadIdFromLoggingThread) {
    minta::detail::OutputTemplate tpl("{threadId}");
    std::thread::id logThreadId;
    std::string rendered;
    std::thread t([&] {
        logThreadId = std::this_thread::get_id();
        auto entry = makeTestEntry(minta::LogLevel::INFO, "msg", "", "", 0, "", {}, logThreadId);
        rendered = tpl.render(entry);
    });
    t.join();

    std::ostringstream oss;
    oss << logThreadId;
    EXPECT_EQ(rendered, oss.str());
    std::ostringstream mainOss;
    mainOss << std::this_thread::get_id();
    EXPECT_NE(rendered, mainOss.str());
}

// setOutputTemplate must be called before logging begins (same contract as
// addSink / setFormatter). This test verifies the template is applied correctly
// when set during initialization, before any log calls.
TEST_F(OutputTemplateTest, SetOutputTemplateBeforeLogging) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>(minta::named("tpl"), "test_concurrent_tpl.txt");
    logger.sink("tpl").outputTemplate("[{level:u3}] {message}");

    for (int i = 0; i < 200; ++i) {
        logger.info("msg {n}", i);
    }
    logger.flush();

    std::string content = TestUtils::readLogFile("test_concurrent_tpl.txt");
    EXPECT_FALSE(content.empty());
    EXPECT_TRUE(content.find("[INF]") != std::string::npos) << "Got: " << content;
}

// ---------------------------------------------------------------------------
// Parser edge cases — unclosed braces, unmatched braces, empty content
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ParseUnclosedBrace) {
    auto segments = minta::detail::parseOutputTemplate("before{unclosed");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "before{unclosed");
}

TEST_F(OutputTemplateTest, ParseUnmatchedSingleCloseBrace) {
    auto segments = minta::detail::parseOutputTemplate("before}after");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "before}after");
}

TEST_F(OutputTemplateTest, ParseEmptyBraces) {
    auto segments = minta::detail::parseOutputTemplate("{}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_TRUE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].literal, "");
}

TEST_F(OutputTemplateTest, ParseUnclosedBraceAfterToken) {
    auto segments = minta::detail::parseOutputTemplate("{message}trail{broken");
    ASSERT_EQ(segments.size(), 2u);
    EXPECT_FALSE(segments[0].isLiteral);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Message);
    EXPECT_TRUE(segments[1].isLiteral);
    EXPECT_EQ(segments[1].literal, "trail{broken");
}

// ---------------------------------------------------------------------------
// Alignment parsing edge cases
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ParseAlignmentNonNumeric) {
    auto segments = minta::detail::parseOutputTemplate("{level,abc}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, 0);
    EXPECT_EQ(segments[0].tokenType, minta::detail::OutputTokenType::Level);
}

TEST_F(OutputTemplateTest, ParseAlignmentOverflow) {
    auto segments = minta::detail::parseOutputTemplate("{level,999999}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, minta::detail::MAX_ALIGNMENT_WIDTH);
}

TEST_F(OutputTemplateTest, ParseAlignmentNegativeOverflow) {
    auto segments = minta::detail::parseOutputTemplate("{level,-999999}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, -minta::detail::MAX_ALIGNMENT_WIDTH);
}

TEST_F(OutputTemplateTest, ParseAlignmentOnlyMinus) {
    auto segments = minta::detail::parseOutputTemplate("{level,-}");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].alignment, 0);
}

TEST_F(OutputTemplateTest, ParseAlignmentDirectEdgeCases) {
    EXPECT_EQ(minta::detail::parseAlignment(""), 0);
    EXPECT_EQ(minta::detail::parseAlignment("-"), 0);
    EXPECT_EQ(minta::detail::parseAlignment("abc"), 0);
    EXPECT_EQ(minta::detail::parseAlignment("12x"), 0);
    EXPECT_EQ(minta::detail::parseAlignment("5"), 5);
    EXPECT_EQ(minta::detail::parseAlignment("-5"), -5);
}

// ---------------------------------------------------------------------------
// Alignment application edge cases
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, AlignmentIntMin) {
    std::string result = minta::detail::applyAlignment("test", INT_MIN);
    EXPECT_EQ(result.size(), static_cast<size_t>(minta::detail::MAX_ALIGNMENT_WIDTH));
    EXPECT_EQ(result.substr(0, 4), "test");
}

TEST_F(OutputTemplateTest, AlignmentClampedLargeWidth) {
    std::string result = minta::detail::applyAlignment("hi", 5000);
    EXPECT_LE(result.size(), static_cast<size_t>(minta::detail::MAX_ALIGNMENT_WIDTH));
}

// ---------------------------------------------------------------------------
// resolveTokenType — unknown tokens
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ResolveTokenTypeUnknown) {
    minta::detail::OutputTokenType type;
    EXPECT_FALSE(minta::detail::resolveTokenType("bogus", type));
    EXPECT_FALSE(minta::detail::resolveTokenType("", type));
    EXPECT_FALSE(minta::detail::resolveTokenType("Timestamp", type));
}

// ---------------------------------------------------------------------------
// Render edge cases
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, RenderLevelUnknownSpec) {
    minta::detail::OutputTemplate tpl("{level:xyz}");
    auto entry = makeTestEntry(minta::LogLevel::WARN, "msg");
    EXPECT_EQ(tpl.render(entry), "WARN");
}

TEST_F(OutputTemplateTest, RenderWithOverrideMessage) {
    minta::detail::OutputTemplate tpl("{message}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "original");
    EXPECT_EQ(tpl.render(entry, "overridden"), "overridden");
}

TEST_F(OutputTemplateTest, RenderSourceFileOnly) {
    minta::detail::OutputTemplate tpl("{source}");
    auto entry = makeTestEntry(minta::LogLevel::INFO, "msg", "", "main.cpp", 42, "");
    EXPECT_EQ(tpl.render(entry), "main.cpp:42");
}

TEST_F(OutputTemplateTest, RenderExceptionWithContent) {
    minta::detail::OutputTemplate tpl("{exception}");
    auto entry = makeTestEntry(minta::LogLevel::ERROR, "Failed");
    entry.exception = minta::detail::make_unique<minta::detail::ExceptionInfo>();
    entry.exception->type = "std::runtime_error";
    entry.exception->message = "Connection refused";
    std::string result = tpl.render(entry);
    EXPECT_EQ(result, "std::runtime_error: Connection refused");
}

TEST_F(OutputTemplateTest, RenderExceptionWithChain) {
    minta::detail::OutputTemplate tpl("{exception}");
    auto entry = makeTestEntry(minta::LogLevel::ERROR, "Failed");
    entry.exception = minta::detail::make_unique<minta::detail::ExceptionInfo>();
    entry.exception->type = "std::runtime_error";
    entry.exception->message = "Connection refused";
    entry.exception->chain = "std::system_error: Network unreachable\nstd::bad_alloc: out of memory";
    std::string result = tpl.render(entry);
    EXPECT_TRUE(result.find("std::runtime_error: Connection refused") != std::string::npos);
    EXPECT_TRUE(result.find("--- std::system_error: Network unreachable") != std::string::npos);
    EXPECT_TRUE(result.find("--- std::bad_alloc: out of memory") != std::string::npos);
}

// ---------------------------------------------------------------------------
// convertTimestampFormat — individual token coverage
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, ConvertTimestampFormatIndividualTokens) {
    EXPECT_EQ(minta::detail::convertTimestampFormat("MM"), "%m");
    EXPECT_EQ(minta::detail::convertTimestampFormat("dd"), "%d");
    EXPECT_EQ(minta::detail::convertTimestampFormat("HH"), "%H");
    EXPECT_EQ(minta::detail::convertTimestampFormat("mm"), "%M");
    EXPECT_EQ(minta::detail::convertTimestampFormat("ss"), "%S");
    EXPECT_EQ(minta::detail::convertTimestampFormat("fff"), std::string(1, '\x01'));
    EXPECT_EQ(minta::detail::convertTimestampFormat("x"), "x");
}

// ---------------------------------------------------------------------------
// OutputTemplate class accessors
// ---------------------------------------------------------------------------

TEST_F(OutputTemplateTest, TemplateStringAccessor) {
    minta::detail::OutputTemplate tpl("{level} {message}");
    EXPECT_EQ(tpl.templateString(), "{level} {message}");
    EXPECT_FALSE(tpl.empty());
}
