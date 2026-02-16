#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <atomic>
#include <vector>

class FilteringTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Per-Sink Level Filtering ---

TEST_F(FilteringTest, PerSinkLevelFiltering) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("errors.log");
    logger.addSink<minta::FileSink>("all.log");

    logger.setSinkLevel(0, minta::LogLevel::ERROR);

    logger.trace("Trace msg");
    logger.debug("Debug msg");
    logger.info("Info msg");
    logger.warn("Warn msg");
    logger.error("Error msg");
    logger.fatal("Fatal msg");

    logger.flush();
    TestUtils::waitForFileContent("errors.log");
    TestUtils::waitForFileContent("all.log");

    std::string errors = TestUtils::readLogFile("errors.log");
    std::string all = TestUtils::readLogFile("all.log");

    EXPECT_EQ(errors.find("Trace msg"), std::string::npos);
    EXPECT_EQ(errors.find("Debug msg"), std::string::npos);
    EXPECT_EQ(errors.find("Info msg"), std::string::npos);
    EXPECT_EQ(errors.find("Warn msg"), std::string::npos);
    EXPECT_NE(errors.find("Error msg"), std::string::npos);
    EXPECT_NE(errors.find("Fatal msg"), std::string::npos);

    EXPECT_NE(all.find("Trace msg"), std::string::npos);
    EXPECT_NE(all.find("Debug msg"), std::string::npos);
    EXPECT_NE(all.find("Info msg"), std::string::npos);
    EXPECT_NE(all.find("Warn msg"), std::string::npos);
    EXPECT_NE(all.find("Error msg"), std::string::npos);
    EXPECT_NE(all.find("Fatal msg"), std::string::npos);
}

TEST_F(FilteringTest, GlobalVsSinkLevelInteraction) {
    minta::LunarLog logger(minta::LogLevel::WARN, false);
    logger.addSink<minta::FileSink>("sink.log");
    logger.setSinkLevel(0, minta::LogLevel::DEBUG);

    logger.debug("Debug filtered by global");
    logger.info("Info filtered by global");
    logger.warn("Warn passes both");
    logger.error("Error passes both");

    logger.flush();
    TestUtils::waitForFileContent("sink.log");
    std::string content = TestUtils::readLogFile("sink.log");

    EXPECT_EQ(content.find("Debug filtered by global"), std::string::npos);
    EXPECT_EQ(content.find("Info filtered by global"), std::string::npos);
    EXPECT_NE(content.find("Warn passes both"), std::string::npos);
    EXPECT_NE(content.find("Error passes both"), std::string::npos);
}

TEST_F(FilteringTest, EffectiveLevelIsMaxOfGlobalAndSink) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("sink.log");
    logger.setSinkLevel(0, minta::LogLevel::ERROR);

    logger.info("Info msg");
    logger.warn("Warn msg");
    logger.error("Error msg");

    logger.flush();
    TestUtils::waitForFileContent("sink.log");
    std::string content = TestUtils::readLogFile("sink.log");

    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_EQ(content.find("Warn msg"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
}

// --- Predicate Filter ---

TEST_F(FilteringTest, GlobalPredicateFilter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("filtered.log");

    logger.setFilter([](const minta::LogEntry& entry) {
        return entry.level >= minta::LogLevel::WARN ||
               entry.customContext.count("important") > 0;
    });

    logger.info("Normal info");
    logger.warn("Warning message");
    logger.setContext("important", "yes");
    logger.info("Important info");

    logger.flush();
    TestUtils::waitForFileContent("filtered.log");
    std::string content = TestUtils::readLogFile("filtered.log");

    EXPECT_EQ(content.find("Normal info"), std::string::npos);
    EXPECT_NE(content.find("Warning message"), std::string::npos);
    EXPECT_NE(content.find("Important info"), std::string::npos);
}

TEST_F(FilteringTest, PerSinkPredicateFilter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("nosensitive.log");
    logger.addSink<minta::FileSink>("all.log");

    logger.setSinkFilter(0, [](const minta::LogEntry& entry) {
        return entry.message.find("sensitive") == std::string::npos;
    });

    logger.info("Normal message");
    logger.info("This has sensitive data");

    logger.flush();
    TestUtils::waitForFileContent("nosensitive.log");
    TestUtils::waitForFileContent("all.log");

    std::string filtered = TestUtils::readLogFile("nosensitive.log");
    std::string all = TestUtils::readLogFile("all.log");

    EXPECT_NE(filtered.find("Normal message"), std::string::npos);
    EXPECT_EQ(filtered.find("sensitive"), std::string::npos);

    EXPECT_NE(all.find("Normal message"), std::string::npos);
    EXPECT_NE(all.find("sensitive"), std::string::npos);
}

TEST_F(FilteringTest, ClearGlobalFilter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.setFilter([](const minta::LogEntry&) { return false; });
    logger.info("Blocked message");
    logger.flush();

    logger.clearFilter();
    logger.info("Allowed message");
    logger.flush();

    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked message"), std::string::npos);
    EXPECT_NE(content.find("Allowed message"), std::string::npos);
}

TEST_F(FilteringTest, ClearSinkFilter) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.setSinkFilter(0, [](const minta::LogEntry&) { return false; });
    logger.info("Blocked");
    logger.flush();

    logger.clearSinkFilter(0);
    logger.info("Allowed");
    logger.flush();

    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked"), std::string::npos);
    EXPECT_NE(content.find("Allowed"), std::string::npos);
}

// --- DSL: Level Comparison ---

TEST_F(FilteringTest, DSLLevelGreaterEqual) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("level >= WARN");

    logger.trace("Trace msg");
    logger.debug("Debug msg");
    logger.info("Info msg");
    logger.warn("Warn msg");
    logger.error("Error msg");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("Debug msg"), std::string::npos);
    EXPECT_EQ(content.find("Info msg"), std::string::npos);
    EXPECT_NE(content.find("Warn msg"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
}

TEST_F(FilteringTest, DSLLevelEqual) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("level == ERROR");

    logger.warn("Warn msg");
    logger.error("Error msg");
    logger.fatal("Fatal msg");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Warn msg"), std::string::npos);
    EXPECT_NE(content.find("Error msg"), std::string::npos);
    EXPECT_EQ(content.find("Fatal msg"), std::string::npos);
}

TEST_F(FilteringTest, DSLLevelNotEqual) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("level != DEBUG");

    logger.trace("Trace msg");
    logger.debug("Debug msg");
    logger.info("Info msg");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("Debug msg"), std::string::npos);
    EXPECT_NE(content.find("Info msg"), std::string::npos);
}

// --- DSL: Message Contains/StartsWith ---

TEST_F(FilteringTest, DSLMessageContains) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("message contains 'error'");

    logger.info("This is an error report");
    logger.info("This is a normal message");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("error report"), std::string::npos);
    EXPECT_EQ(content.find("normal message"), std::string::npos);
}

TEST_F(FilteringTest, DSLMessageStartsWith) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("message startswith 'ALERT'");

    logger.info("ALERT: something happened");
    logger.info("Normal: nothing happened");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("ALERT: something happened"), std::string::npos);
    EXPECT_EQ(content.find("Normal: nothing happened"), std::string::npos);
}

// --- DSL: Context Has/Equals ---

TEST_F(FilteringTest, DSLContextHas) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("context has 'request_id'");

    logger.info("No context");
    logger.setContext("request_id", "abc123");
    logger.info("With context");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("No context"), std::string::npos);
    EXPECT_NE(content.find("With context"), std::string::npos);
}

TEST_F(FilteringTest, DSLContextKeyEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("context env == 'production'");

    logger.setContext("env", "development");
    logger.info("Dev message");
    logger.setContext("env", "production");
    logger.info("Prod message");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Dev message"), std::string::npos);
    EXPECT_NE(content.find("Prod message"), std::string::npos);
}

// --- DSL: Template Matching ---

TEST_F(FilteringTest, DSLTemplateEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("template == 'User {username} logged in'");

    logger.info("User {username} logged in", "alice");
    logger.info("User {username} logged out", "bob");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("alice logged in"), std::string::npos);
    EXPECT_EQ(content.find("bob logged out"), std::string::npos);
}

TEST_F(FilteringTest, DSLTemplateContains) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("template contains 'logged in'");

    logger.info("User {username} logged in", "alice");
    logger.info("User {username} logged out", "bob");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("alice logged in"), std::string::npos);
    EXPECT_EQ(content.find("bob logged out"), std::string::npos);
}

// --- DSL: Negation ---

TEST_F(FilteringTest, DSLNegation) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("not message contains 'heartbeat'");

    logger.info("heartbeat ping");
    logger.info("Important event");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("heartbeat"), std::string::npos);
    EXPECT_NE(content.find("Important event"), std::string::npos);
}

TEST_F(FilteringTest, DSLNegatedLevelEquals) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("not level == TRACE");

    logger.trace("Trace msg");
    logger.debug("Debug msg");
    logger.info("Info msg");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_NE(content.find("Debug msg"), std::string::npos);
    EXPECT_NE(content.find("Info msg"), std::string::npos);
}

// --- DSL: Multiple Rules (AND Combination) ---

TEST_F(FilteringTest, DSLMultipleRulesANDCombined) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("level >= INFO");
    logger.addFilterRule("not message contains 'debug'");

    logger.trace("Trace msg");
    logger.info("Info with debug info");
    logger.info("Info clean message");
    logger.warn("Warn msg");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Trace msg"), std::string::npos);
    EXPECT_EQ(content.find("debug info"), std::string::npos);
    EXPECT_NE(content.find("Info clean message"), std::string::npos);
    EXPECT_NE(content.find("Warn msg"), std::string::npos);
}

// --- DSL: Invalid Rule ---

TEST_F(FilteringTest, DSLInvalidRuleThrows) {
    EXPECT_THROW(minta::FilterRule::parse(""), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("garbage rule"), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("level > WARN"), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("level >= UNKNOWN"), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("message blah 'text'"), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("message contains unquoted"), std::invalid_argument);
    EXPECT_THROW(minta::FilterRule::parse("not "), std::invalid_argument);
}

TEST_F(FilteringTest, DSLValidRulesDoNotThrow) {
    EXPECT_NO_THROW(minta::FilterRule::parse("level >= TRACE"));
    EXPECT_NO_THROW(minta::FilterRule::parse("level == FATAL"));
    EXPECT_NO_THROW(minta::FilterRule::parse("level != INFO"));
    EXPECT_NO_THROW(minta::FilterRule::parse("message contains 'text'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("message startswith 'prefix'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("context has 'key'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("context mykey == 'value'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("template == 'template str'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("template contains 'partial'"));
    EXPECT_NO_THROW(minta::FilterRule::parse("not level >= WARN"));
    EXPECT_NO_THROW(minta::FilterRule::parse("not message contains 'x'"));
}

// --- DSL: Clear Rules ---

TEST_F(FilteringTest, DSLClearFilterRules) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("level >= ERROR");
    logger.info("Blocked by rule");
    logger.flush();

    logger.clearFilterRules();
    logger.info("Allowed after clear");
    logger.flush();

    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked by rule"), std::string::npos);
    EXPECT_NE(content.find("Allowed after clear"), std::string::npos);
}

// --- Filter + Operators Interaction ---

TEST_F(FilteringTest, FilterWithOperators) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addFilterRule("message contains 'alice'");

    logger.info("User {@id} is {$name}", 42, "alice");
    logger.info("User {@id} is {$name}", 99, "bob");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("alice"), std::string::npos);
    EXPECT_EQ(content.find("bob"), std::string::npos);
}

// --- Filter + Template Hash Interaction ---

TEST_F(FilteringTest, FilterWithTemplateHash) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("test_log.txt");

    logger.addFilterRule("template contains 'logged in'");

    logger.info("User {name} logged in", "alice");
    logger.info("User {name} logged out", "bob");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_NE(content.find("templateHash"), std::string::npos);
    EXPECT_NE(content.find("alice"), std::string::npos);
    EXPECT_EQ(content.find("bob"), std::string::npos);
}

// --- Runtime Filter Change ---

TEST_F(FilteringTest, RuntimeFilterChange) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.setSinkLevel(0, minta::LogLevel::ERROR);
    logger.info("Blocked at ERROR level");
    logger.error("Passes at ERROR level");
    logger.flush();

    logger.setSinkLevel(0, minta::LogLevel::TRACE);
    logger.info("Passes after level change");
    logger.flush();

    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked at ERROR level"), std::string::npos);
    EXPECT_NE(content.find("Passes at ERROR level"), std::string::npos);
    EXPECT_NE(content.find("Passes after level change"), std::string::npos);
}

// --- Per-Sink DSL Rules ---

TEST_F(FilteringTest, PerSinkDSLRules) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("filtered.log");
    logger.addSink<minta::FileSink>("all.log");

    logger.addSinkFilterRule(0, "not message contains 'noisy'");

    logger.info("Important event");
    logger.info("noisy heartbeat");

    logger.flush();
    TestUtils::waitForFileContent("filtered.log");
    TestUtils::waitForFileContent("all.log");

    std::string filtered = TestUtils::readLogFile("filtered.log");
    std::string all = TestUtils::readLogFile("all.log");

    EXPECT_NE(filtered.find("Important event"), std::string::npos);
    EXPECT_EQ(filtered.find("noisy"), std::string::npos);

    EXPECT_NE(all.find("Important event"), std::string::npos);
    EXPECT_NE(all.find("noisy"), std::string::npos);
}

TEST_F(FilteringTest, ClearSinkFilterRules) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.addSinkFilterRule(0, "level >= ERROR");
    logger.info("Blocked");
    logger.flush();

    logger.clearSinkFilterRules(0);
    logger.info("Allowed");
    logger.flush();

    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked"), std::string::npos);
    EXPECT_NE(content.find("Allowed"), std::string::npos);
}

// --- Full Pipeline Order Test ---

TEST_F(FilteringTest, FullFilterPipeline) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.setFilter([](const minta::LogEntry& entry) {
        return entry.message.find("blocked_global") == std::string::npos;
    });

    logger.setSinkLevel(0, minta::LogLevel::WARN);

    logger.setSinkFilter(0, [](const minta::LogEntry& entry) {
        return entry.message.find("blocked_sink") == std::string::npos;
    });

    logger.addSinkFilterRule(0, "not message contains 'blocked_dsl'");

    logger.debug("Blocked by global level");
    logger.info("Blocked by sink level");
    logger.warn("blocked_global predicate");
    logger.warn("blocked_sink predicate");
    logger.warn("blocked_dsl rule");
    logger.warn("This passes everything");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string content = TestUtils::readLogFile("test_log.txt");

    EXPECT_EQ(content.find("Blocked by global level"), std::string::npos);
    EXPECT_EQ(content.find("Blocked by sink level"), std::string::npos);
    EXPECT_EQ(content.find("blocked_global predicate"), std::string::npos);
    EXPECT_EQ(content.find("blocked_sink predicate"), std::string::npos);
    EXPECT_EQ(content.find("blocked_dsl rule"), std::string::npos);
    EXPECT_NE(content.find("This passes everything"), std::string::npos);
}

// --- Thread Safety: Change Filter While Logging ---

TEST_F(FilteringTest, ThreadSafeFilterChange) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    std::atomic<bool> done(false);
    const int iterations = 200;

    std::thread filterChanger([&]() {
        for (int i = 0; i < iterations && !done.load(); ++i) {
            logger.setFilter([](const minta::LogEntry& entry) {
                return entry.level >= minta::LogLevel::WARN;
            });
            logger.clearFilter();
            logger.setSinkLevel(0, minta::LogLevel::ERROR);
            logger.setSinkLevel(0, minta::LogLevel::TRACE);
            logger.addFilterRule("level >= INFO");
            logger.clearFilterRules();
        }
    });

    std::thread logWriter([&]() {
        for (int i = 0; i < iterations * 5 && !done.load(); ++i) {
            logger.info("Message {n}", i);
        }
    });

    logWriter.join();
    done.store(true);
    filterChanger.join();
    logger.flush();

    SUCCEED();
}

TEST_F(FilteringTest, ThreadSafeSinkFilterChange) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    std::atomic<bool> done(false);
    const int iterations = 200;

    std::thread filterChanger([&]() {
        for (int i = 0; i < iterations && !done.load(); ++i) {
            logger.setSinkFilter(0, [](const minta::LogEntry& entry) {
                return entry.level >= minta::LogLevel::WARN;
            });
            logger.clearSinkFilter(0);
            logger.addSinkFilterRule(0, "level >= INFO");
            logger.clearSinkFilterRules(0);
        }
    });

    std::thread logWriter([&]() {
        for (int i = 0; i < iterations * 5 && !done.load(); ++i) {
            logger.info("Message {n}", i);
        }
    });

    logWriter.join();
    done.store(true);
    filterChanger.join();
    logger.flush();

    SUCCEED();
}
