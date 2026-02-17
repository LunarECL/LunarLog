#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class TagRoutingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Tag parsing tests ---

TEST_F(TagRoutingTest, ParseSingleTag) {
    auto result = minta::detail::parseTags("[metrics] Request took 100ms");
    EXPECT_EQ(result.first.size(), 1u);
    EXPECT_EQ(result.first[0], "metrics");
    EXPECT_EQ(result.second, "Request took 100ms");
}

TEST_F(TagRoutingTest, ParseMultipleTags) {
    auto result = minta::detail::parseTags("[audit][security] Admin action");
    EXPECT_EQ(result.first.size(), 2u);
    EXPECT_EQ(result.first[0], "audit");
    EXPECT_EQ(result.first[1], "security");
    EXPECT_EQ(result.second, "Admin action");
}

TEST_F(TagRoutingTest, ParseNoTags) {
    auto result = minta::detail::parseTags("Normal message");
    EXPECT_TRUE(result.first.empty());
    EXPECT_EQ(result.second, "Normal message");
}

TEST_F(TagRoutingTest, ParseTagWithHyphensUnderscores) {
    auto result = minta::detail::parseTags("[my-tag_2] Message");
    EXPECT_EQ(result.first.size(), 1u);
    EXPECT_EQ(result.first[0], "my-tag_2");
    EXPECT_EQ(result.second, "Message");
}

TEST_F(TagRoutingTest, ParseInvalidTagChars) {
    // Tags with spaces or special chars should stop parsing
    auto result = minta::detail::parseTags("[invalid tag] Message");
    EXPECT_TRUE(result.first.empty());
    EXPECT_EQ(result.second, "[invalid tag] Message");
}

TEST_F(TagRoutingTest, ParseEmptyBrackets) {
    auto result = minta::detail::parseTags("[] Message");
    EXPECT_TRUE(result.first.empty());
    EXPECT_EQ(result.second, "[] Message");
}

TEST_F(TagRoutingTest, ParseUnclosedBracket) {
    auto result = minta::detail::parseTags("[unclosed Message");
    EXPECT_TRUE(result.first.empty());
    EXPECT_EQ(result.second, "[unclosed Message");
}

TEST_F(TagRoutingTest, ParseTagsNotAtStart) {
    auto result = minta::detail::parseTags("Prefix [tag] message");
    EXPECT_TRUE(result.first.empty());
    EXPECT_EQ(result.second, "Prefix [tag] message");
}

// --- Tag routing to sinks ---

TEST_F(TagRoutingTest, OnlyTagRouting) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("metrics", "test_metrics.txt");
    logger.addSink<minta::FileSink>("all", "test_all_tags.txt");
    logger.sink("metrics").only("metrics");
    
    logger.info("[metrics] Request count: {count}", 42);
    logger.info("Regular log message");
    logger.flush();
    
    TestUtils::waitForFileContent("test_metrics.txt");
    TestUtils::waitForFileContent("test_all_tags.txt");
    
    std::string metricsContent = TestUtils::readLogFile("test_metrics.txt");
    std::string allContent = TestUtils::readLogFile("test_all_tags.txt");
    
    EXPECT_TRUE(metricsContent.find("Request count: 42") != std::string::npos);
    EXPECT_TRUE(metricsContent.find("Regular log message") == std::string::npos);
    
    EXPECT_TRUE(allContent.find("Regular log message") != std::string::npos);
    // Untagged sink receives logs without tags
}

TEST_F(TagRoutingTest, ExceptTagRouting) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("no-debug", "test_no_debug.txt");
    logger.sink("no-debug").except("debug");
    
    logger.info("[debug] Debug info");
    logger.info("Normal message");
    logger.flush();
    
    TestUtils::waitForFileContent("test_no_debug.txt");
    std::string content = TestUtils::readLogFile("test_no_debug.txt");
    
    EXPECT_TRUE(content.find("Debug info") == std::string::npos);
    EXPECT_TRUE(content.find("Normal message") != std::string::npos);
}

TEST_F(TagRoutingTest, OnlyTakesPrecedenceOverExcept) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("mixed", "test_mixed_tags.txt");
    logger.sink("mixed").only("audit").except("audit");
    
    logger.info("[audit] Audit event");
    logger.flush();
    
    TestUtils::waitForFileContent("test_mixed_tags.txt");
    std::string content = TestUtils::readLogFile("test_mixed_tags.txt");
    
    // only() takes precedence, so audit tag matches only() -> accepted
    EXPECT_TRUE(content.find("Audit event") != std::string::npos);
}

TEST_F(TagRoutingTest, NoTagsGoToSinksWithoutOnlyFilter) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("specific", "test_specific.txt");
    logger.addSink<minta::FileSink>("general", "test_general.txt");
    logger.sink("specific").only("metrics");
    
    logger.info("No tags here");
    logger.flush();
    
    TestUtils::waitForFileContent("test_general.txt");
    std::string specificContent = TestUtils::readLogFile("test_specific.txt");
    std::string generalContent = TestUtils::readLogFile("test_general.txt");
    
    EXPECT_TRUE(specificContent.find("No tags here") == std::string::npos);
    EXPECT_TRUE(generalContent.find("No tags here") != std::string::npos);
}

TEST_F(TagRoutingTest, MultipleOnlyTags) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("combo", "test_combo.txt");
    logger.sink("combo").only("audit").only("security");
    
    logger.info("[audit] Audit event");
    logger.info("[security] Security event");
    logger.info("[other] Other event");
    logger.flush();
    
    TestUtils::waitForFileContent("test_combo.txt");
    std::string content = TestUtils::readLogFile("test_combo.txt");
    
    EXPECT_TRUE(content.find("Audit event") != std::string::npos);
    EXPECT_TRUE(content.find("Security event") != std::string::npos);
    EXPECT_TRUE(content.find("Other event") == std::string::npos);
}

TEST_F(TagRoutingTest, MultiTagMessage) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("audit", "test_audit.txt");
    logger.sink("audit").only("audit");
    
    logger.info("[audit][security] Multi-tag event");
    logger.flush();
    
    TestUtils::waitForFileContent("test_audit.txt");
    std::string content = TestUtils::readLogFile("test_audit.txt");
    EXPECT_TRUE(content.find("Multi-tag event") != std::string::npos);
}

TEST_F(TagRoutingTest, TagsStrippedFromHumanReadable) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("human", "test_human_tags.txt");
    
    logger.info("[metrics] Request took {ms}ms", 100);
    logger.flush();
    
    TestUtils::waitForFileContent("test_human_tags.txt");
    std::string content = TestUtils::readLogFile("test_human_tags.txt");
    
    // Message should not contain [metrics] prefix
    EXPECT_TRUE(content.find("[metrics]") == std::string::npos);
    EXPECT_TRUE(content.find("Request took 100ms") != std::string::npos);
}

TEST_F(TagRoutingTest, TagsInJsonOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json", "test_json_tags.txt");
    
    logger.info("[metrics][perf] Latency {ms}ms", 50);
    logger.flush();
    
    TestUtils::waitForFileContent("test_json_tags.txt");
    std::string content = TestUtils::readLogFile("test_json_tags.txt");
    
    EXPECT_TRUE(content.find("\"tags\":[\"metrics\",\"perf\"]") != std::string::npos);
    EXPECT_TRUE(content.find("Latency 50ms") != std::string::npos);
}

TEST_F(TagRoutingTest, TagsInXmlOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("xml", "test_xml_tags.txt");
    
    logger.info("[audit] Event occurred");
    logger.flush();
    
    TestUtils::waitForFileContent("test_xml_tags.txt");
    std::string content = TestUtils::readLogFile("test_xml_tags.txt");
    
    EXPECT_TRUE(content.find("<tags><tag>audit</tag></tags>") != std::string::npos);
}

TEST_F(TagRoutingTest, ClearTagFilters) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("clear", "test_clear_tags.txt");
    logger.sink("clear").only("audit");
    logger.sink("clear").clearTagFilters();
    
    logger.info("Should now be visible");
    logger.flush();
    
    TestUtils::waitForFileContent("test_clear_tags.txt");
    std::string content = TestUtils::readLogFile("test_clear_tags.txt");
    EXPECT_TRUE(content.find("Should now be visible") != std::string::npos);
}

TEST_F(TagRoutingTest, ExceptMultipleTags) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("exc", "test_exc.txt");
    logger.sink("exc").except("debug").except("trace");
    
    logger.info("[debug] Debug msg");
    logger.info("[trace] Trace msg");
    logger.info("[audit] Audit msg");
    logger.info("Normal msg");
    logger.flush();
    
    TestUtils::waitForFileContent("test_exc.txt");
    std::string content = TestUtils::readLogFile("test_exc.txt");
    
    EXPECT_TRUE(content.find("Debug msg") == std::string::npos);
    EXPECT_TRUE(content.find("Trace msg") == std::string::npos);
    EXPECT_TRUE(content.find("Audit msg") != std::string::npos);
    EXPECT_TRUE(content.find("Normal msg") != std::string::npos);
}

TEST_F(TagRoutingTest, TagWithPlaceholders) {
    auto result = minta::detail::parseTags("[metrics] Request {method} took {ms}ms");
    EXPECT_EQ(result.first.size(), 1u);
    EXPECT_EQ(result.first[0], "metrics");
    EXPECT_EQ(result.second, "Request {method} took {ms}ms");
}

TEST_F(TagRoutingTest, NoTagsNoFilterAllSinksReceive) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("s1", "test_notag1.txt");
    logger.addSink<minta::FileSink>("s2", "test_notag2.txt");
    
    logger.info("Broadcast message");
    logger.flush();
    
    TestUtils::waitForFileContent("test_notag1.txt");
    TestUtils::waitForFileContent("test_notag2.txt");
    
    EXPECT_TRUE(TestUtils::readLogFile("test_notag1.txt").find("Broadcast message") != std::string::npos);
    EXPECT_TRUE(TestUtils::readLogFile("test_notag2.txt").find("Broadcast message") != std::string::npos);
}

TEST_F(TagRoutingTest, TagRoutingCombinedWithLevel) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("metrics-warn", "test_metrics_warn.txt");
    logger.sink("metrics-warn").only("metrics").level(minta::LogLevel::WARN);
    
    logger.info("[metrics] Info metric");
    logger.warn("[metrics] Warn metric");
    logger.info("Normal info");
    logger.flush();
    
    TestUtils::waitForFileContent("test_metrics_warn.txt");
    std::string content = TestUtils::readLogFile("test_metrics_warn.txt");
    
    EXPECT_TRUE(content.find("Info metric") == std::string::npos);
    EXPECT_TRUE(content.find("Warn metric") != std::string::npos);
    EXPECT_TRUE(content.find("Normal info") == std::string::npos);
}

TEST_F(TagRoutingTest, OnlyTagNumericName) {
    auto result = minta::detail::parseTags("[tag123] Message");
    EXPECT_EQ(result.first.size(), 1u);
    EXPECT_EQ(result.first[0], "tag123");
}

TEST_F(TagRoutingTest, NestedBracketsNotParsed) {
    // Brackets inside content should not parse
    auto result = minta::detail::parseTags("[[inner]] Message");
    // First [ starts, second [ is invalid char for tag
    EXPECT_TRUE(result.first.empty());
}

TEST_F(TagRoutingTest, TagsWithArgumentsFormatted) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("fmtarg", "test_fmtarg.txt");
    
    logger.info("[perf] Processed {count} items in {time}s", 100, 2.5);
    logger.flush();
    
    TestUtils::waitForFileContent("test_fmtarg.txt");
    std::string content = TestUtils::readLogFile("test_fmtarg.txt");
    EXPECT_TRUE(content.find("Processed 100 items in 2.5s") != std::string::npos);
    EXPECT_TRUE(content.find("[perf]") == std::string::npos);
}
