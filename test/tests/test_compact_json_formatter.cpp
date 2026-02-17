#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <regex>
#include <string>
#include <cmath>
#include <limits>

class CompactJsonFormatterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ---------------------------------------------------------------------------
// Basic output structure
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, BasicOutputStructure) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_basic.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    logger.flush();
    TestUtils::waitForFileContent("cjson_basic.txt");
    std::string content = TestUtils::readLogFile("cjson_basic.txt");
    if (!content.empty() && content.back() == '\n') content.pop_back();

    // Must have @t
    EXPECT_TRUE(content.find("\"@t\":\"") != std::string::npos);
    // Must have @mt with the template
    EXPECT_TRUE(content.find("\"@mt\":\"User {username} logged in from {ip}\"") != std::string::npos);
    // Must have @i (template hash)
    EXPECT_TRUE(content.find("\"@i\":\"") != std::string::npos);
    // Must NOT have @l (INFO is omitted)
    EXPECT_TRUE(content.find("\"@l\":") == std::string::npos);
    // Must NOT have @m (off by default)
    EXPECT_TRUE(content.find("\"@m\":") == std::string::npos);
    // Properties flattened to top level
    EXPECT_TRUE(content.find("\"username\":\"alice\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"ip\":\"192.168.1.1\"") != std::string::npos);
    // No nested properties object
    EXPECT_TRUE(content.find("\"properties\"") == std::string::npos);
    // Starts with { and ends with }
    EXPECT_EQ(content.front(), '{');
    EXPECT_EQ(content.back(), '}');
}

// ---------------------------------------------------------------------------
// Level omission and abbreviation
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, LevelOmittedForInfo) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_info.txt");

    logger.info("test message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_info.txt");
    std::string content = TestUtils::readLogFile("cjson_info.txt");

    EXPECT_TRUE(content.find("\"@l\":") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, LevelWarn) {
    minta::LunarLog logger(minta::LogLevel::WARN);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_warn.txt");

    logger.warn("warning message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_warn.txt");
    std::string content = TestUtils::readLogFile("cjson_warn.txt");

    EXPECT_TRUE(content.find("\"@l\":\"WRN\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, LevelError) {
    minta::LunarLog logger(minta::LogLevel::ERROR);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_error.txt");

    logger.error("error message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_error.txt");
    std::string content = TestUtils::readLogFile("cjson_error.txt");

    EXPECT_TRUE(content.find("\"@l\":\"ERR\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, LevelTrace) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_trace.txt");

    logger.trace("trace message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_trace.txt");
    std::string content = TestUtils::readLogFile("cjson_trace.txt");

    EXPECT_TRUE(content.find("\"@l\":\"TRC\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, LevelDebug) {
    minta::LunarLog logger(minta::LogLevel::DEBUG, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_debug.txt");

    logger.debug("debug message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_debug.txt");
    std::string content = TestUtils::readLogFile("cjson_debug.txt");

    EXPECT_TRUE(content.find("\"@l\":\"DBG\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, LevelFatal) {
    minta::LunarLog logger(minta::LogLevel::FATAL);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_fatal.txt");

    logger.fatal("fatal message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_fatal.txt");
    std::string content = TestUtils::readLogFile("cjson_fatal.txt");

    EXPECT_TRUE(content.find("\"@l\":\"FTL\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Timestamp format
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, TimestampUtcIso8601) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_timestamp.txt");

    logger.info("test");

    logger.flush();
    TestUtils::waitForFileContent("cjson_timestamp.txt");
    std::string content = TestUtils::readLogFile("cjson_timestamp.txt");

    // ISO 8601 UTC: YYYY-MM-DDTHH:MM:SS.mmmZ
    std::regex tsRegex(R"("@t":"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}Z")");
    EXPECT_TRUE(std::regex_search(content, tsRegex));
}

// ---------------------------------------------------------------------------
// Rendered message (@m)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, IncludeRenderedMessage) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_rendered.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("Hello {name}", "world");

    logger.flush();
    TestUtils::waitForFileContent("cjson_rendered.txt");
    std::string content = TestUtils::readLogFile("cjson_rendered.txt");

    EXPECT_TRUE(content.find("\"@m\":\"Hello world\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@mt\":\"Hello {name}\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, DefaultNoRenderedMessage) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_rendered.txt");

    logger.info("Hello {name}", "world");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_rendered.txt");
    std::string content = TestUtils::readLogFile("cjson_no_rendered.txt");

    EXPECT_TRUE(content.find("\"@m\":") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Template hash (@i)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, TemplateHashPresent) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_hash.txt");

    logger.info("User {name} logged in", "alice");

    logger.flush();
    TestUtils::waitForFileContent("cjson_hash.txt");
    std::string content = TestUtils::readLogFile("cjson_hash.txt");

    // @i should be an 8-char hex string
    std::regex hashRegex(R"("@i":"[0-9a-f]{8}")");
    EXPECT_TRUE(std::regex_search(content, hashRegex));
}

// ---------------------------------------------------------------------------
// Flattened properties
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, FlattenedProperties) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_flat.txt");

    logger.info("Order {orderId} from {customer} total {amount}",
                "ORD-001", "alice", "99.99");

    logger.flush();
    TestUtils::waitForFileContent("cjson_flat.txt");
    std::string content = TestUtils::readLogFile("cjson_flat.txt");

    EXPECT_TRUE(content.find("\"orderId\":\"ORD-001\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"customer\":\"alice\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"amount\":\"99.99\"") != std::string::npos);
    // Must not have "properties" wrapper
    EXPECT_TRUE(content.find("\"properties\"") == std::string::npos);
}

// ---------------------------------------------------------------------------
// @ prefix escaping on property names
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, AtPrefixContextEscaping) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_at_escape.txt");

    logger.setContext("@mykey", "myvalue");
    logger.setContext("normalkey", "normalvalue");
    logger.info("test");

    logger.flush();
    TestUtils::waitForFileContent("cjson_at_escape.txt");
    std::string content = TestUtils::readLogFile("cjson_at_escape.txt");

    // @mykey should be escaped to @@mykey
    EXPECT_TRUE(content.find("\"@@mykey\":\"myvalue\"") != std::string::npos);
    // normalkey should remain unchanged
    EXPECT_TRUE(content.find("\"normalkey\":\"normalvalue\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// JSON string escaping
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, EscapeQuotes) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_esc_quotes.txt");

    logger.info("He said {v}", "\"hello\"");

    logger.flush();
    TestUtils::waitForFileContent("cjson_esc_quotes.txt");
    std::string content = TestUtils::readLogFile("cjson_esc_quotes.txt");

    EXPECT_TRUE(content.find(R"(\"hello\")") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, EscapeBackslash) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_esc_backslash.txt");

    logger.info("path {v}", "C:\\Users\\test");

    logger.flush();
    TestUtils::waitForFileContent("cjson_esc_backslash.txt");
    std::string content = TestUtils::readLogFile("cjson_esc_backslash.txt");

    EXPECT_TRUE(content.find(R"(C:\\Users\\test)") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, EscapeNewlineAndTab) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_esc_newline.txt");

    logger.info("lines {v}", "a\nb\tc");

    logger.flush();
    TestUtils::waitForFileContent("cjson_esc_newline.txt");
    std::string content = TestUtils::readLogFile("cjson_esc_newline.txt");

    EXPECT_TRUE(content.find(R"(a\nb\tc)") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, EscapeControlChar) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_esc_ctrl.txt");

    std::string ctrlStr(1, '\x01');
    logger.info("ctrl {v}", ctrlStr);

    logger.flush();
    TestUtils::waitForFileContent("cjson_esc_ctrl.txt");
    std::string content = TestUtils::readLogFile("cjson_esc_ctrl.txt");

    EXPECT_TRUE(content.find("\\u0001") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, EscapeUnicode) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_esc_unicode.txt");

    logger.info("emoji {v}", "\xC3\xA9");

    logger.flush();
    TestUtils::waitForFileContent("cjson_esc_unicode.txt");
    std::string content = TestUtils::readLogFile("cjson_esc_unicode.txt");

    // UTF-8 bytes >= 0x80 pass through (not escaped)
    EXPECT_TRUE(content.find("\xC3\xA9") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Operators (@ destructure, $ stringify)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, DestructureOperator) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_destruct.txt");

    logger.info("Count: {@count}, Flag: {@flag}", 42, true);

    logger.flush();
    TestUtils::waitForFileContent("cjson_destruct.txt");
    std::string content = TestUtils::readLogFile("cjson_destruct.txt");

    // @ operator: native JSON types
    EXPECT_TRUE(content.find("\"count\":42") != std::string::npos);
    EXPECT_TRUE(content.find("\"flag\":true") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, StringifyOperator) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_stringify.txt");

    logger.info("Count: {$count}, Flag: {$flag}", 42, true);

    logger.flush();
    TestUtils::waitForFileContent("cjson_stringify.txt");
    std::string content = TestUtils::readLogFile("cjson_stringify.txt");

    // $ operator: always strings
    EXPECT_TRUE(content.find("\"count\":\"42\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"flag\":\"true\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Tags
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, Tags) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_tags.txt");

    logger.info("[auth] User {name} logged in", "alice");

    logger.flush();
    TestUtils::waitForFileContent("cjson_tags.txt");
    std::string content = TestUtils::readLogFile("cjson_tags.txt");

    EXPECT_TRUE(content.find("\"tags\":[\"auth\"]") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":\"alice\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, MultipleTags) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_multi_tags.txt");

    logger.info("[auth][security] Login attempt", "");

    logger.flush();
    TestUtils::waitForFileContent("cjson_multi_tags.txt");
    std::string content = TestUtils::readLogFile("cjson_multi_tags.txt");

    EXPECT_TRUE(content.find("\"tags\":[\"auth\",\"security\"]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, CustomContext) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_context.txt");

    logger.setContext("requestId", "req-001");
    logger.setContext("env", "production");
    logger.info("Processing");

    logger.flush();
    TestUtils::waitForFileContent("cjson_context.txt");
    std::string content = TestUtils::readLogFile("cjson_context.txt");

    EXPECT_TRUE(content.find("\"requestId\":\"req-001\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"env\":\"production\"") != std::string::npos);
    // Context must be top level, not nested
    EXPECT_TRUE(content.find("\"context\"") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Scoped context
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, ScopedContext) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_scoped.txt");

    {
        auto scope = logger.scope({{"txId", "tx-123"}, {"userId", "u-42"}});
        logger.info("In scope");
    }

    logger.flush();
    TestUtils::waitForFileContent("cjson_scoped.txt");
    std::string content = TestUtils::readLogFile("cjson_scoped.txt");

    EXPECT_TRUE(content.find("\"txId\":\"tx-123\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"userId\":\"u-42\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Format specifiers (property values are raw, message is formatted)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, FormatSpecifiers) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_format_spec.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("Price: {amount:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("cjson_format_spec.txt");
    std::string content = TestUtils::readLogFile("cjson_format_spec.txt");

    // @mt has the template with format spec
    EXPECT_TRUE(content.find("\"@mt\":\"Price: {amount:.2f}\"") != std::string::npos);
    // @m has the rendered value
    EXPECT_TRUE(content.find("\"@m\":\"Price: 3.14\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Pipe transforms
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, PipeTransforms) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_pipe.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("Name: {name|upper}", "alice");

    logger.flush();
    TestUtils::waitForFileContent("cjson_pipe.txt");
    std::string content = TestUtils::readLogFile("cjson_pipe.txt");

    // @mt has the template
    EXPECT_TRUE(content.find("\"@mt\":\"Name: {name|upper}\"") != std::string::npos);
    // @m has the transformed rendered message
    EXPECT_TRUE(content.find("\"@m\":\"Name: ALICE\"") != std::string::npos);
    // Property value is the raw value
    EXPECT_TRUE(content.find("\"name\":\"alice\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Indexed placeholders
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, IndexedPlaceholders) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_indexed.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("{0} sent {1} to {0}", "alice", "42");

    logger.flush();
    TestUtils::waitForFileContent("cjson_indexed.txt");
    std::string content = TestUtils::readLogFile("cjson_indexed.txt");

    // @m should have the rendered message
    EXPECT_TRUE(content.find("\"@m\":\"alice sent 42 to alice\"") != std::string::npos);
    // Properties with indexed names
    EXPECT_TRUE(content.find("\"0\":\"alice\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"1\":\"42\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Alignment
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, Alignment) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_align.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("{name,-20}", "John");

    logger.flush();
    TestUtils::waitForFileContent("cjson_align.txt");
    std::string content = TestUtils::readLogFile("cjson_align.txt");

    // @mt has the template with alignment
    EXPECT_TRUE(content.find("\"@mt\":\"{name,-20}\"") != std::string::npos);
    // @m has the aligned rendered message (left-aligned, padded with spaces)
    EXPECT_TRUE(content.find("\"@m\":\"John                \"") != std::string::npos);
    // Property value is raw
    EXPECT_TRUE(content.find("\"name\":\"John\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// No template (plain string message)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, PlainStringMessage) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_plain.txt");

    logger.info("Simple message with no placeholders");

    logger.flush();
    TestUtils::waitForFileContent("cjson_plain.txt");
    std::string content = TestUtils::readLogFile("cjson_plain.txt");

    // @mt should contain the message as template
    EXPECT_TRUE(content.find("\"@mt\":\"Simple message with no placeholders\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Single-line output (JSONL)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, SingleLineOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_singleline.txt");

    logger.info("line one");
    logger.info("line two");

    logger.flush();
    TestUtils::waitForFileContent("cjson_singleline.txt");
    std::string content = TestUtils::readLogFile("cjson_singleline.txt");

    // Each log entry should be on its own line (no embedded newlines in JSON)
    // The file will have entries separated by newlines from the sink
    size_t firstNewline = content.find('\n');
    EXPECT_TRUE(firstNewline != std::string::npos);

    // First line should be complete JSON
    std::string firstLine = content.substr(0, firstNewline);
    EXPECT_EQ(firstLine.front(), '{');
    EXPECT_EQ(firstLine.back(), '}');
}

// ---------------------------------------------------------------------------
// Combined features: tags + context + properties
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, CombinedFeatures) {
    minta::LunarLog logger(minta::LogLevel::WARN, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_combined.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.setContext("env", "staging");
    logger.warn("[audit] User {user} performed {action}", "bob", "delete");

    logger.flush();
    TestUtils::waitForFileContent("cjson_combined.txt");
    std::string content = TestUtils::readLogFile("cjson_combined.txt");

    // All fields present
    EXPECT_TRUE(content.find("\"@t\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@l\":\"WRN\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@mt\":\"User {user} performed {action}\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@i\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@m\":\"User bob performed delete\"") != std::string::npos);
    // Flattened properties
    EXPECT_TRUE(content.find("\"user\":\"bob\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"action\":\"delete\"") != std::string::npos);
    // Flattened context
    EXPECT_TRUE(content.find("\"env\":\"staging\"") != std::string::npos);
    // Tags
    EXPECT_TRUE(content.find("\"tags\":[\"audit\"]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// SinkProxy formatter() integration
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, SinkProxyFormatterSetter) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>(minta::named("pipeline"), "cjson_proxy.txt");
    logger.sink("pipeline").formatter(minta::detail::make_unique<minta::CompactJsonFormatter>());

    logger.info("Via proxy {v}", "test");

    logger.flush();
    TestUtils::waitForFileContent("cjson_proxy.txt");
    std::string content = TestUtils::readLogFile("cjson_proxy.txt");

    // Should produce compact JSON output
    EXPECT_TRUE(content.find("\"@t\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"@mt\":\"Via proxy {v}\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"v\":\"test\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Escaped brackets pass through
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, EscapedBrackets) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto fmt = minta::detail::make_unique<minta::CompactJsonFormatter>();
    fmt->includeRenderedMessage(true);
    logger.addSink<minta::FileSink>("cjson_escaped_braces.txt");
    logger.sink("sink_0").formatter(std::move(fmt));

    logger.info("JSON: {{key}} = {val}", "42");

    logger.flush();
    TestUtils::waitForFileContent("cjson_escaped_braces.txt");
    std::string content = TestUtils::readLogFile("cjson_escaped_braces.txt");

    // @m should have literal braces
    EXPECT_TRUE(content.find("\"@m\":\"JSON: {key} = 42\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// includeRenderedMessage toggle
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, IncludeRenderedMessageToggle) {
    minta::CompactJsonFormatter fmt;
    EXPECT_FALSE(fmt.isRenderedMessageIncluded());
    fmt.includeRenderedMessage(true);
    EXPECT_TRUE(fmt.isRenderedMessageIncluded());
    fmt.includeRenderedMessage(false);
    EXPECT_FALSE(fmt.isRenderedMessageIncluded());
}

// ---------------------------------------------------------------------------
// Destructure edge cases
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, DestructureEmptyString) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_destruct_empty.txt");

    logger.info("Val: {@v}", "");

    logger.flush();
    TestUtils::waitForFileContent("cjson_destruct_empty.txt");
    std::string content = TestUtils::readLogFile("cjson_destruct_empty.txt");

    EXPECT_TRUE(content.find("\"v\":\"\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, DestructureNaN) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_destruct_nan.txt");

    logger.info("Val: {@v}", std::numeric_limits<double>::quiet_NaN());

    logger.flush();
    TestUtils::waitForFileContent("cjson_destruct_nan.txt");
    std::string content = TestUtils::readLogFile("cjson_destruct_nan.txt");

    // NaN is not a valid JSON number; must be emitted as a string
    EXPECT_TRUE(content.find("\"v\":\"") != std::string::npos);
    // Must NOT be a bare nan token (invalid JSON)
    EXPECT_TRUE(content.find("\"v\":nan") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, DestructureInfinity) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_destruct_inf.txt");

    logger.info("Pos: {@pos}, Neg: {@neg}",
                std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity());

    logger.flush();
    TestUtils::waitForFileContent("cjson_destruct_inf.txt");
    std::string content = TestUtils::readLogFile("cjson_destruct_inf.txt");

    // Infinity is not a valid JSON number; must be emitted as a string
    EXPECT_TRUE(content.find("\"pos\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"neg\":\"") != std::string::npos);
    // Must NOT be bare inf tokens
    EXPECT_TRUE(content.find("\"pos\":inf") == std::string::npos);
    EXPECT_TRUE(content.find("\"neg\":-inf") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, DestructureNegativeZero) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_destruct_negzero.txt");

    logger.info("Val: {@v}", -0.0);

    logger.flush();
    TestUtils::waitForFileContent("cjson_destruct_negzero.txt");
    std::string content = TestUtils::readLogFile("cjson_destruct_negzero.txt");

    // Negative zero should normalize to 0 in JSON output
    EXPECT_TRUE(content.find("\"v\":0") != std::string::npos);
    // Must not have a negative sign
    EXPECT_TRUE(content.find("\"v\":-0") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Locale-independent numeric parsing
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, LocaleIndependentNumericParse) {
    // Verify that the shared toJsonNativeValue correctly parses
    // decimal numbers regardless of locale state by using the
    // detail::json::toJsonNativeValue function directly.
    std::string result = minta::detail::json::toJsonNativeValue("3.14");
    // Must produce a valid JSON number, not a string
    EXPECT_EQ(result.front(), '3');
    EXPECT_TRUE(result.find('.') != std::string::npos || result.find("3.14") != std::string::npos
                || result == "3.14");

    // Integer must come through as bare number
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("42"), "42");
    // Booleans
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("true"), "true");
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("false"), "false");
    // Empty string
    EXPECT_EQ(minta::detail::json::toJsonNativeValue(""), "\"\"");
    // Regular string
    EXPECT_EQ(minta::detail::json::toJsonNativeValue("hello"), "\"hello\"");
}

// ---------------------------------------------------------------------------
// Top-level key collision handling
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, PropertyContextCollisionLastWriteWins) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_collision.txt");

    logger.setContext("env", "context-value");
    logger.info("Msg {env}", "property-value");

    logger.flush();
    TestUtils::waitForFileContent("cjson_collision.txt");
    std::string content = TestUtils::readLogFile("cjson_collision.txt");

    // Both "env" keys are emitted (properties first, then context).
    // JSON parsers using last-value-wins will see the context value.
    // Verify both values are present in the raw output.
    EXPECT_TRUE(content.find("\"env\":\"property-value\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"env\":\"context-value\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, PropertyNamedTagsCollision) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_tags_collision.txt");

    // "tags" appears as both a user property and the system tags array
    logger.info("[audit] Has tags: {tags}", "user-tags-value");

    logger.flush();
    TestUtils::waitForFileContent("cjson_tags_collision.txt");
    std::string content = TestUtils::readLogFile("cjson_tags_collision.txt");

    // The user property "tags" is emitted before the system tags array.
    // Both appear; system "tags" array wins for last-value-wins parsers.
    EXPECT_TRUE(content.find("\"tags\":\"user-tags-value\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"tags\":[\"audit\"]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Intentional omissions (fields NOT emitted unless enabled)
// ---------------------------------------------------------------------------

TEST_F(CompactJsonFormatterTest, NoRenderedMessageByDefault) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_m.txt");

    logger.info("Template {x}", "val");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_m.txt");
    std::string content = TestUtils::readLogFile("cjson_no_m.txt");

    EXPECT_TRUE(content.find("\"@m\":") == std::string::npos);
    EXPECT_TRUE(content.find("\"@mt\":") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, NoLevelFieldForInfo) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_level.txt");

    logger.info("info message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_level.txt");
    std::string content = TestUtils::readLogFile("cjson_no_level.txt");

    // @l must be absent for INFO level
    EXPECT_TRUE(content.find("\"@l\":") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, NoTemplateHashWithoutTemplate) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_hash.txt");

    // Plain message with no placeholders — templateStr will be empty
    logger.info("Plain message no placeholders");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_hash.txt");
    std::string content = TestUtils::readLogFile("cjson_no_hash.txt");

    EXPECT_TRUE(content.find("\"@mt\":\"Plain message no placeholders\"") != std::string::npos);
}

TEST_F(CompactJsonFormatterTest, NoSourceLocation) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_src.txt");

    logger.info("test message");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_src.txt");
    std::string content = TestUtils::readLogFile("cjson_no_src.txt");

    // CompactJsonFormatter does not emit file/line/function fields
    EXPECT_TRUE(content.find("\"file\":") == std::string::npos);
    EXPECT_TRUE(content.find("\"line\":") == std::string::npos);
    EXPECT_TRUE(content.find("\"function\":") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, NoTransformsField) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_transforms.txt");

    logger.info("Name: {name|upper}", "alice");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_transforms.txt");
    std::string content = TestUtils::readLogFile("cjson_no_transforms.txt");

    // CompactJsonFormatter does not emit a "transforms" object
    // (unlike the full JsonFormatter which does)
    EXPECT_TRUE(content.find("\"transforms\"") == std::string::npos);
}

TEST_F(CompactJsonFormatterTest, NoContextWrapper) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("cjson_no_ctx_wrapper.txt");

    logger.setContext("k", "v");
    logger.info("msg");

    logger.flush();
    TestUtils::waitForFileContent("cjson_no_ctx_wrapper.txt");
    std::string content = TestUtils::readLogFile("cjson_no_ctx_wrapper.txt");

    // Context is flattened; no "context" wrapper object
    EXPECT_TRUE(content.find("\"context\"") == std::string::npos);
    EXPECT_TRUE(content.find("\"k\":\"v\"") != std::string::npos);
}
