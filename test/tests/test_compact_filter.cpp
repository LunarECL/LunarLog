#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>

class CompactFilterTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(CompactFilterTest, EmptyExpressionReturnsNoRules) {
    auto rules = minta::detail::parseCompactFilter("");
    EXPECT_TRUE(rules.empty());
}

TEST_F(CompactFilterTest, WhitespaceOnlyReturnsNoRules) {
    auto rules = minta::detail::parseCompactFilter("   ");
    EXPECT_TRUE(rules.empty());
}

TEST_F(CompactFilterTest, SingleLevelPlusToken) {
    auto rules = minta::detail::parseCompactFilter("WARN+");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, LevelPlusCaseInsensitive) {
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("warn+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("Warn+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("wArN+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("TRACE+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("debug+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("info+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("error+"));
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("fatal+"));
}

TEST_F(CompactFilterTest, InvalidLevelPlusThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("UNKNOWN+"), std::invalid_argument);
    // WARNING+ is now accepted as alias for WARN+
    EXPECT_NO_THROW(minta::detail::parseCompactFilter("WARNING+"));
}

TEST_F(CompactFilterTest, TildeKeyword) {
    auto rules = minta::detail::parseCompactFilter("~error");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, NegatedTildeKeyword) {
    auto rules = minta::detail::parseCompactFilter("!~heartbeat");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, ContextHasKey) {
    auto rules = minta::detail::parseCompactFilter("ctx:request_id");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, ContextKeyEqualsValue) {
    auto rules = minta::detail::parseCompactFilter("ctx:env=production");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, TemplateEquals) {
    auto rules = minta::detail::parseCompactFilter("tpl:\"User {name} logged in\"");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, TemplateEqualsNoSpaces) {
    auto rules = minta::detail::parseCompactFilter("tpl:heartbeat");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, NegatedTemplateEquals) {
    auto rules = minta::detail::parseCompactFilter("!tpl:\"heartbeat {status}\"");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, MultipleTokensANDCombined) {
    auto rules = minta::detail::parseCompactFilter("WARN+ ~error !~heartbeat");
    ASSERT_EQ(rules.size(), 3u);
}

TEST_F(CompactFilterTest, QuotedKeywordDoubleQuotes) {
    auto rules = minta::detail::parseCompactFilter("~\"connection reset\"");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, QuotedKeywordSingleQuotes) {
    auto rules = minta::detail::parseCompactFilter("~'connection reset'");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, QuotedContextValue) {
    auto rules = minta::detail::parseCompactFilter("ctx:env=\"some value\"");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, QuotedContextValueSingleQuotes) {
    auto rules = minta::detail::parseCompactFilter("ctx:env='some value'");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, UnrecognizedTokenThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("garbage"), std::invalid_argument);
    EXPECT_THROW(minta::detail::parseCompactFilter("foo:bar"), std::invalid_argument);
}

TEST_F(CompactFilterTest, NegatedQuotedKeyword) {
    auto rules = minta::detail::parseCompactFilter("!~\"connection reset\"");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, GlobalFilterLevelPlus) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_level.txt");

    logger.filter("WARN+");

    logger.trace("Trace msg");
    logger.debug("Debug msg");
    logger.info("Info msg");
    logger.warn("Warn msg");
    logger.error("Error msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_level.txt");
    std::string content = TestUtils::readLogFile("compact_level.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("Debug msg"), std::string::npos);
    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_NE(content.find("Warn msg"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterLevelPlusCaseInsensitive) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_level.txt");

    logger.filter("error+");

    logger.warn("Warn msg");
    logger.error("Error msg");
    logger.fatal("Fatal msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_level.txt");
    std::string content = TestUtils::readLogFile("compact_level.txt");

    EXPECT_EQ(content.find("Warn msg"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
    EXPECT_NE(content.find("Fatal msg"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterMessageContains) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_msg.txt");

    logger.filter("~error");

    logger.info("This is an error report");
    logger.info("This is a normal message");

    logger.flush();
    TestUtils::waitForFileContent("compact_msg.txt");
    std::string content = TestUtils::readLogFile("compact_msg.txt");

    EXPECT_NE(content.find("error report"), std::string::npos);
    EXPECT_EQ(content.find("normal message"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterMessageNotContains) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_notmsg.txt");

    logger.filter("!~heartbeat");

    logger.info("heartbeat ping");
    logger.info("Important event");

    logger.flush();
    TestUtils::waitForFileContent("compact_notmsg.txt");
    std::string content = TestUtils::readLogFile("compact_notmsg.txt");

    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Important event"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterContextHasKey) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxhas.txt");

    logger.filter("ctx:request_id");

    logger.info("No context");
    logger.setContext("request_id", "abc123");
    logger.info("With context");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxhas.txt");
    std::string content = TestUtils::readLogFile("compact_ctxhas.txt");

    EXPECT_EQ(content.find("No context"), std::string::npos);
    EXPECT_NE(content.find("With context"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterContextKeyEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxeq.txt");

    logger.filter("ctx:env=production");

    logger.setContext("env", "development");
    logger.info("Dev message");
    logger.setContext("env", "production");
    logger.info("Prod message");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxeq.txt");
    std::string content = TestUtils::readLogFile("compact_ctxeq.txt");

    EXPECT_EQ(content.find("Dev message"), std::string::npos);
    EXPECT_NE(content.find("Prod message"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterTemplateEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_tpl.txt");

    logger.filter("tpl:\"User {username} logged in\"");

    logger.info("User {username} logged in", "alice");
    logger.info("User {username} logged out", "bob");

    logger.flush();
    TestUtils::waitForFileContent("compact_tpl.txt");
    std::string content = TestUtils::readLogFile("compact_tpl.txt");

    EXPECT_NE(content.find("alice logged in"), std::string::npos);
    EXPECT_EQ(content.find("bob logged out"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterNegatedTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_nottpl.txt");

    logger.filter("!tpl:\"heartbeat {status}\"");

    logger.info("heartbeat {status}", "ok");
    logger.info("User {name} logged in", "alice");

    logger.flush();
    TestUtils::waitForFileContent("compact_nottpl.txt");
    std::string content = TestUtils::readLogFile("compact_nottpl.txt");

    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("alice logged in"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterMultipleTokensANDCombined) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_multi.txt");

    logger.filter("INFO+ !~heartbeat");

    logger.trace("Trace msg");
    logger.info("heartbeat ping");
    logger.info("Important event");
    logger.warn("Warn msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_multi.txt");
    std::string content = TestUtils::readLogFile("compact_multi.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Important event"), std::string::npos);
    EXPECT_NE(content.find("Warn msg"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterQuotedKeywordWithSpaces) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_quoted.txt");

    logger.filter("~\"connection reset\"");

    logger.info("connection reset by peer");
    logger.info("connection established");
    logger.info("some other message");

    logger.flush();
    TestUtils::waitForFileContent("compact_quoted.txt");
    std::string content = TestUtils::readLogFile("compact_quoted.txt");

    EXPECT_NE(content.find("connection reset by peer"), std::string::npos);
    EXPECT_EQ(content.find("connection established"), std::string::npos);
    EXPECT_EQ(content.find("some other message"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterQuotedContextValue) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxquoted.txt");

    logger.filter("ctx:env=\"east coast\"");

    logger.setContext("env", "west coast");
    logger.info("West message");
    logger.setContext("env", "east coast");
    logger.info("East message");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxquoted.txt");
    std::string content = TestUtils::readLogFile("compact_ctxquoted.txt");

    EXPECT_EQ(content.find("West message"), std::string::npos);
    EXPECT_NE(content.find("East message"), std::string::npos);
}

TEST_F(CompactFilterTest, GlobalFilterClearAfterCompact) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_clear.txt");

    logger.filter("ERROR+");
    logger.info("Blocked by compact filter");
    logger.flush();

    logger.clearFilterRules();
    logger.info("Allowed after clear");
    logger.flush();

    TestUtils::waitForFileContent("compact_clear.txt");
    std::string content = TestUtils::readLogFile("compact_clear.txt");

    EXPECT_EQ(content.find("Blocked by compact filter"), std::string::npos);
    EXPECT_NE(content.find("Allowed after clear"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterLevelPlus) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_level.txt");
    logger.addSink<minta::FileSink>(minta::named("all"), "compact_sink_all.txt");

    logger.sink("filtered").filter("WARN+");

    logger.info("Info msg");
    logger.warn("Warn msg");
    logger.error("Error msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_level.txt");
    TestUtils::waitForFileContent("compact_sink_all.txt");

    std::string filtered = TestUtils::readLogFile("compact_sink_level.txt");
    std::string all = TestUtils::readLogFile("compact_sink_all.txt");

    EXPECT_EQ(filtered.find("Info msg"), std::string::npos);
    EXPECT_NE(filtered.find("Warn msg"), std::string::npos);
    EXPECT_NE(filtered.find("Error msg"), std::string::npos);

    EXPECT_NE(all.find("Info msg"), std::string::npos);
    EXPECT_NE(all.find("Warn msg"), std::string::npos);
    EXPECT_NE(all.find("Error msg"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterMessageContains) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_msg.txt");

    logger.sink("filtered").filter("!~noisy");

    logger.info("Important event");
    logger.info("noisy heartbeat");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_msg.txt");
    std::string content = TestUtils::readLogFile("compact_sink_msg.txt");

    EXPECT_NE(content.find("Important event"), std::string::npos);
    EXPECT_EQ(content.find("noisy"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterMultipleTokens) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_multi.txt");

    logger.sink("filtered").filter("WARN+ !~heartbeat");

    logger.info("Info msg");
    logger.warn("heartbeat ping");
    logger.warn("Important warning");
    logger.error("Error msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_multi.txt");
    std::string content = TestUtils::readLogFile("compact_sink_multi.txt");

    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Important warning"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterContextHas) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_ctx.txt");

    logger.sink("filtered").filter("ctx:session_id");

    logger.info("No session");
    logger.setContext("session_id", "s-123");
    logger.info("Has session");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_ctx.txt");
    std::string content = TestUtils::readLogFile("compact_sink_ctx.txt");

    EXPECT_EQ(content.find("No session"), std::string::npos);
    EXPECT_NE(content.find("Has session"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterContextKeyEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_ctxeq.txt");

    logger.sink("filtered").filter("ctx:role=admin");

    logger.setContext("role", "user");
    logger.info("User msg");
    logger.setContext("role", "admin");
    logger.info("Admin msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_ctxeq.txt");
    std::string content = TestUtils::readLogFile("compact_sink_ctxeq.txt");

    EXPECT_EQ(content.find("User msg"), std::string::npos);
    EXPECT_NE(content.find("Admin msg"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_tpl.txt");

    logger.sink("filtered").filter("tpl:\"User {name} logged in\"");

    logger.info("User {name} logged in", "alice");
    logger.info("User {name} logged out", "bob");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_tpl.txt");
    std::string content = TestUtils::readLogFile("compact_sink_tpl.txt");

    EXPECT_NE(content.find("alice logged in"), std::string::npos);
    EXPECT_EQ(content.find("bob logged out"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyFilterNegatedTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("filtered"), "compact_sink_nottpl.txt");

    logger.sink("filtered").filter("!tpl:\"heartbeat {status}\"");

    logger.info("heartbeat {status}", "ok");
    logger.info("Event {type} occurred", "login");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_nottpl.txt");
    std::string content = TestUtils::readLogFile("compact_sink_nottpl.txt");

    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("login occurred"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyChaining) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("out"), "compact_sink_chain.txt");

    logger.sink("out")
        .filter("WARN+")
        .filter("!~heartbeat");

    logger.info("Info msg");
    logger.warn("heartbeat warn");
    logger.warn("Real warning");

    logger.flush();
    TestUtils::waitForFileContent("compact_sink_chain.txt");
    std::string content = TestUtils::readLogFile("compact_sink_chain.txt");

    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Real warning"), std::string::npos);
}

TEST_F(CompactFilterTest, AllLevelPlusVariants) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_trace.txt");

    logger.filter("trace+");

    logger.trace("Trace msg");
    logger.info("Info msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_trace.txt");
    std::string content = TestUtils::readLogFile("compact_trace.txt");

    EXPECT_NE(content.find("Trace msg"), std::string::npos);
    EXPECT_NE(content.find("Info msg"), std::string::npos);
}

TEST_F(CompactFilterTest, FatalPlusOnlyFatal) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_fatal.txt");

    logger.filter("FATAL+");

    logger.error("Error msg");
    logger.fatal("Fatal msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_fatal.txt");
    std::string content = TestUtils::readLogFile("compact_fatal.txt");

    EXPECT_EQ(content.find("Error msg"), std::string::npos);
    EXPECT_NE(content.find("Fatal msg"), std::string::npos);
}

TEST_F(CompactFilterTest, KeywordIsCaseSensitive) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_casesens.txt");

    logger.filter("~Error");

    logger.info("This has Error in it");
    logger.info("This has error in it");
    logger.info("No match here");

    logger.flush();
    TestUtils::waitForFileContent("compact_casesens.txt");
    std::string content = TestUtils::readLogFile("compact_casesens.txt");

    EXPECT_NE(content.find("This has Error in it"), std::string::npos);
    EXPECT_EQ(content.find("This has error in it"), std::string::npos);
    EXPECT_EQ(content.find("No match here"), std::string::npos);
}

TEST_F(CompactFilterTest, MixedCompactAndDSLFilters) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_mixed.txt");

    logger.addFilterRule("level >= INFO");
    logger.filter("!~heartbeat");

    logger.trace("Trace msg");
    logger.info("heartbeat ping");
    logger.info("Important event");
    logger.warn("Warn msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_mixed.txt");
    std::string content = TestUtils::readLogFile("compact_mixed.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Important event"), std::string::npos);
    EXPECT_NE(content.find("Warn msg"), std::string::npos);
}

TEST_F(CompactFilterTest, EmptyCompactFilterIsNoOp) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_empty.txt");

    logger.filter("");

    logger.info("Passes through");

    logger.flush();
    TestUtils::waitForFileContent("compact_empty.txt");
    std::string content = TestUtils::readLogFile("compact_empty.txt");

    EXPECT_NE(content.find("Passes through"), std::string::npos);
}

TEST_F(CompactFilterTest, ContextKeyWithSpecialChars) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxspecial.txt");

    logger.filter("ctx:x-request-id");

    logger.info("No header");
    logger.setContext("x-request-id", "req-abc");
    logger.info("With header");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxspecial.txt");
    std::string content = TestUtils::readLogFile("compact_ctxspecial.txt");

    EXPECT_EQ(content.find("No header"), std::string::npos);
    EXPECT_NE(content.find("With header"), std::string::npos);
}

TEST_F(CompactFilterTest, ContextKeyWithDots) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxdots.txt");

    logger.filter("ctx:user.name=alice");

    logger.setContext("user.name", "bob");
    logger.info("Bob msg");
    logger.setContext("user.name", "alice");
    logger.info("Alice msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxdots.txt");
    std::string content = TestUtils::readLogFile("compact_ctxdots.txt");

    EXPECT_EQ(content.find("Bob msg"), std::string::npos);
    EXPECT_NE(content.find("Alice msg"), std::string::npos);
}

TEST_F(CompactFilterTest, ThreeWayANDCombination) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_3way.txt");

    logger.filter("WARN+ ~critical ctx:env=production");

    logger.setContext("env", "production");
    logger.info("critical but INFO level");
    logger.warn("routine production warn");
    logger.warn("critical production warn");

    logger.setContext("env", "staging");
    logger.warn("critical staging warn");

    logger.flush();
    TestUtils::waitForFileContent("compact_3way.txt");
    std::string content = TestUtils::readLogFile("compact_3way.txt");

    EXPECT_EQ(content.find("critical but INFO level"), std::string::npos);
    EXPECT_EQ(content.find("routine production warn"), std::string::npos);
    EXPECT_NE(content.find("critical production warn"), std::string::npos);
    EXPECT_EQ(content.find("critical staging warn"), std::string::npos);
}

TEST_F(CompactFilterTest, QuotedNegatedKeywordWithSpaces) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_negquoted.txt");

    logger.filter("!~\"connection reset\"");

    logger.info("connection reset by peer");
    logger.info("connection established");
    logger.info("other message");

    logger.flush();
    TestUtils::waitForFileContent("compact_negquoted.txt");
    std::string content = TestUtils::readLogFile("compact_negquoted.txt");

    EXPECT_EQ(content.find("connection reset by peer"), std::string::npos);
    EXPECT_NE(content.find("connection established"), std::string::npos);
    EXPECT_NE(content.find("other message"), std::string::npos);
}

TEST_F(CompactFilterTest, QuotedTemplateWithSpaces) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_tplquoted.txt");

    logger.filter("tpl:\"User {name} logged in\"");

    logger.info("User {name} logged in", "alice");
    logger.info("User {name} signed up", "bob");

    logger.flush();
    TestUtils::waitForFileContent("compact_tplquoted.txt");
    std::string content = TestUtils::readLogFile("compact_tplquoted.txt");

    EXPECT_NE(content.find("alice logged in"), std::string::npos);
    EXPECT_EQ(content.find("bob signed up"), std::string::npos);
}

TEST_F(CompactFilterTest, SinkProxyClearAfterCompactFilter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>(minta::named("out"), "compact_sink_clear.txt");

    logger.sink("out").filter("ERROR+");
    logger.info("Blocked");
    logger.flush();

    logger.sink("out").clearFilterRules();
    logger.info("Allowed");
    logger.flush();

    TestUtils::waitForFileContent("compact_sink_clear.txt");
    std::string content = TestUtils::readLogFile("compact_sink_clear.txt");

    EXPECT_EQ(content.find("Blocked"), std::string::npos);
    EXPECT_NE(content.find("Allowed"), std::string::npos);
}

TEST_F(CompactFilterTest, MultipleFilterCallsStack) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_stack.txt");

    logger.filter("WARN+");
    logger.filter("!~heartbeat");

    logger.info("Info msg");
    logger.warn("heartbeat ping");
    logger.warn("Real warning");

    logger.flush();
    TestUtils::waitForFileContent("compact_stack.txt");
    std::string content = TestUtils::readLogFile("compact_stack.txt");

    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Real warning"), std::string::npos);
}

TEST_F(CompactFilterTest, QuotedSingleQuoteKeyword) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_sqkw.txt");

    logger.filter("~'connection reset'");

    logger.info("connection reset by peer");
    logger.info("some other message");

    logger.flush();
    TestUtils::waitForFileContent("compact_sqkw.txt");
    std::string content = TestUtils::readLogFile("compact_sqkw.txt");

    EXPECT_NE(content.find("connection reset by peer"), std::string::npos);
    EXPECT_EQ(content.find("some other message"), std::string::npos);
}

TEST_F(CompactFilterTest, ContextValueQuotedSingleQuotes) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_ctxsq.txt");

    logger.filter("ctx:env='east coast'");

    logger.setContext("env", "west coast");
    logger.info("West msg");
    logger.setContext("env", "east coast");
    logger.info("East msg");

    logger.flush();
    TestUtils::waitForFileContent("compact_ctxsq.txt");
    std::string content = TestUtils::readLogFile("compact_ctxsq.txt");

    EXPECT_EQ(content.find("West msg"), std::string::npos);
    EXPECT_NE(content.find("East msg"), std::string::npos);
}

TEST_F(CompactFilterTest, CompactFilterWithJsonFormatter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("compact_json.txt");

    logger.filter("WARN+ ~critical");

    logger.info("critical but low level");
    logger.warn("critical warning alert");
    logger.warn("routine warning only");

    logger.flush();
    TestUtils::waitForFileContent("compact_json.txt");
    std::string content = TestUtils::readLogFile("compact_json.txt");

    EXPECT_EQ(content.find("critical but low level"), std::string::npos);
    EXPECT_NE(content.find("critical warning alert"), std::string::npos);
    EXPECT_EQ(content.find("routine warning only"), std::string::npos);
    EXPECT_NE(content.find("\"level\""), std::string::npos);
}

TEST_F(CompactFilterTest, LeadingTrailingWhitespaceInExpression) {
    auto rules = minta::detail::parseCompactFilter("  WARN+  ~error  ");
    ASSERT_EQ(rules.size(), 2u);
}

TEST_F(CompactFilterTest, TabSeparatedTokens) {
    auto rules = minta::detail::parseCompactFilter("WARN+\t~error\t!~heartbeat");
    ASSERT_EQ(rules.size(), 3u);
}

// --- H1: Single quote in value throws ---

TEST_F(CompactFilterTest, SingleQuoteInKeywordThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("~it's"), std::invalid_argument);
}

TEST_F(CompactFilterTest, SingleQuoteInCtxValueThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("ctx:key=it's"), std::invalid_argument);
}

// --- M2: Unterminated quote throws ---

TEST_F(CompactFilterTest, UnterminatedDoubleQuoteThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("~\"unterminated"), std::invalid_argument);
}

TEST_F(CompactFilterTest, UnterminatedSingleQuoteThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("~'unterminated"), std::invalid_argument);
}

// --- L1: WARNING+ alias ---

TEST_F(CompactFilterTest, WarningPlusAlias) {
    auto rules = minta::detail::parseCompactFilter("WARNING+");
    ASSERT_EQ(rules.size(), 1u);
    // Should behave like WARN+
}

TEST_F(CompactFilterTest, WarningPlusIntegration) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_warning_alias.txt");
    logger.filter("WARNING+");

    logger.info("Should be filtered");
    logger.warn("Should pass");

    logger.flush();
    TestUtils::waitForFileContent("compact_warning_alias.txt");
    std::string content = TestUtils::readLogFile("compact_warning_alias.txt");
    EXPECT_EQ(content.find("Should be filtered"), std::string::npos);
    EXPECT_NE(content.find("Should pass"), std::string::npos);
}

// --- H2: Quoted context key gets stripped ---

TEST_F(CompactFilterTest, QuotedContextKeyStripped) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("compact_quoted_ctx_key.txt");
    logger.setContext("myKey", "myVal");
    logger.filter("ctx:\"myKey\"=\"myVal\"");

    logger.info("With context");

    logger.flush();
    TestUtils::waitForFileContent("compact_quoted_ctx_key.txt");
    std::string content = TestUtils::readLogFile("compact_quoted_ctx_key.txt");
    EXPECT_NE(content.find("With context"), std::string::npos);

    logger.clearAllContext();
}

// --- M1: Quoted has-key stripped ---

TEST_F(CompactFilterTest, QuotedHasKeyStripped) {
    auto rules = minta::detail::parseCompactFilter("ctx:\"userId\"");
    ASSERT_EQ(rules.size(), 1u);
    // Should match context key "userId" (without quotes)
}

// --- L2: Special characters in values ---

TEST_F(CompactFilterTest, BackslashInKeyword) {
    auto rules = minta::detail::parseCompactFilter("~path\\\\to\\\\file");
    ASSERT_EQ(rules.size(), 1u);
}

TEST_F(CompactFilterTest, NewlineInQuotedKeyword) {
    // Newlines inside quotes should work (no single quotes)
    auto rules = minta::detail::parseCompactFilter("~\"line1\\nline2\"");
    ASSERT_EQ(rules.size(), 1u);
}

// --- L3: Bare tpl: and !tpl: boundary ---

TEST_F(CompactFilterTest, BareTplColonThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("tpl:"), std::invalid_argument);
}

TEST_F(CompactFilterTest, BareNegatedTplColonThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("!tpl:"), std::invalid_argument);
}

// --- R2: Empty keyword throws ---

TEST_F(CompactFilterTest, EmptyQuotedKeywordThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("~\"\""), std::invalid_argument);
}

TEST_F(CompactFilterTest, EmptyNegatedQuotedKeywordThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("!~\"\""), std::invalid_argument);
}

// --- R2: Bare ctx: throws specific error ---

TEST_F(CompactFilterTest, BareCtxColonThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("ctx:"), std::invalid_argument);
}

// --- R3: Empty context value throws ---

TEST_F(CompactFilterTest, EmptyContextValueThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("ctx:key="), std::invalid_argument);
}

TEST_F(CompactFilterTest, EmptyQuotedContextValueThrows) {
    EXPECT_THROW(minta::detail::parseCompactFilter("ctx:key=\"\""), std::invalid_argument);
}
