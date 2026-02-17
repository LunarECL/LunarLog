#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>

class NamedSinksTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(NamedSinksTest, AddNamedSinkBasic) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("myfile", "test_named1.txt");
    logger.info("Hello named sink");
    logger.flush();
    TestUtils::waitForFileContent("test_named1.txt");
    std::string content = TestUtils::readLogFile("test_named1.txt");
    EXPECT_TRUE(content.find("Hello named sink") != std::string::npos);
}

TEST_F(NamedSinksTest, DuplicateNameThrows) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("dup", "test_dup1.txt");
    EXPECT_THROW(logger.addSink<minta::FileSink>("dup", "test_dup2.txt"), std::invalid_argument);
}

TEST_F(NamedSinksTest, UnknownNameThrows) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    EXPECT_THROW(logger.sink("nonexistent"), std::invalid_argument);
}

TEST_F(NamedSinksTest, SinkProxySetLevel) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("errors", "test_errors.txt");
    logger.sink("errors").level(minta::LogLevel::ERROR);
    logger.info("Should not appear");
    logger.error("Should appear");
    logger.flush();
    TestUtils::waitForFileContent("test_errors.txt");
    std::string content = TestUtils::readLogFile("test_errors.txt");
    EXPECT_TRUE(content.find("Should not appear") == std::string::npos);
    EXPECT_TRUE(content.find("Should appear") != std::string::npos);
}

TEST_F(NamedSinksTest, SinkProxyChaining) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("chained", "test_chained.txt");
    logger.sink("chained").level(minta::LogLevel::WARN);
    logger.debug("Debug msg");
    logger.warn("Warn msg");
    logger.flush();
    TestUtils::waitForFileContent("test_chained.txt");
    std::string content = TestUtils::readLogFile("test_chained.txt");
    EXPECT_TRUE(content.find("Debug msg") == std::string::npos);
    EXPECT_TRUE(content.find("Warn msg") != std::string::npos);
}

TEST_F(NamedSinksTest, AutoNamedSinks) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("test_auto0.txt");
    // First unnamed sink gets auto-name "sink_0"
    logger.sink("sink_0").level(minta::LogLevel::ERROR);
    logger.info("Should not appear");
    logger.error("Error appears");
    logger.flush();
    TestUtils::waitForFileContent("test_auto0.txt");
    std::string content = TestUtils::readLogFile("test_auto0.txt");
    EXPECT_TRUE(content.find("Should not appear") == std::string::npos);
    EXPECT_TRUE(content.find("Error appears") != std::string::npos);
}

TEST_F(NamedSinksTest, NamedSinkWithFormatter) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("jsonfile", "test_named_json.txt");
    logger.info("JSON named {val}", 42);
    logger.flush();
    TestUtils::waitForFileContent("test_named_json.txt");
    std::string content = TestUtils::readLogFile("test_named_json.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(content.find("JSON named 42") != std::string::npos);
}

TEST_F(NamedSinksTest, MultipleNamedSinks) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("all", "test_all.txt");
    logger.addSink<minta::FileSink>("errors", "test_err_only.txt");
    logger.sink("errors").level(minta::LogLevel::ERROR);
    logger.info("Info message");
    logger.error("Error message");
    logger.flush();
    TestUtils::waitForFileContent("test_all.txt");
    TestUtils::waitForFileContent("test_err_only.txt");
    std::string allContent = TestUtils::readLogFile("test_all.txt");
    std::string errContent = TestUtils::readLogFile("test_err_only.txt");
    EXPECT_TRUE(allContent.find("Info message") != std::string::npos);
    EXPECT_TRUE(allContent.find("Error message") != std::string::npos);
    EXPECT_TRUE(errContent.find("Info message") == std::string::npos);
    EXPECT_TRUE(errContent.find("Error message") != std::string::npos);
}

TEST_F(NamedSinksTest, IndexBasedApiStillWorks) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_idx.txt");
    logger.setSinkLevel(0, minta::LogLevel::WARN);
    logger.info("Should not appear");
    logger.warn("Should appear");
    logger.flush();
    TestUtils::waitForFileContent("test_idx.txt");
    std::string content = TestUtils::readLogFile("test_idx.txt");
    EXPECT_TRUE(content.find("Should not appear") == std::string::npos);
    EXPECT_TRUE(content.find("Should appear") != std::string::npos);
}

TEST_F(NamedSinksTest, SinkProxyFilterRule) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("filtered", "test_filtered.txt");
    logger.sink("filtered").filterRule("level >= WARN");
    logger.debug("Debug should be filtered");
    logger.warn("Warn should pass");
    logger.flush();
    TestUtils::waitForFileContent("test_filtered.txt");
    std::string content = TestUtils::readLogFile("test_filtered.txt");
    EXPECT_TRUE(content.find("Debug should be filtered") == std::string::npos);
    EXPECT_TRUE(content.find("Warn should pass") != std::string::npos);
}

TEST_F(NamedSinksTest, ClearFiltersOnProxy) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("clr", "test_clr.txt");
    logger.sink("clr").only("metrics");
    logger.sink("clr").clearTagFilters();
    // After clearTagFilters, tag filters are cleared so untagged msgs should pass
    logger.info("Info after clear");
    logger.flush();
    TestUtils::waitForFileContent("test_clr.txt");
    std::string content = TestUtils::readLogFile("test_clr.txt");
    EXPECT_TRUE(content.find("Info after clear") != std::string::npos);
}

TEST_F(NamedSinksTest, DefaultConsoleSinkAutoNamed) {
    // Default console sink should be auto-named sink_0
    minta::LunarLog logger(minta::LogLevel::INFO, true);
    // Should not throw — default sink exists as sink_0
    logger.sink("sink_0").level(minta::LogLevel::ERROR);
    // Just testing it doesn't throw
    SUCCEED();
}

TEST_F(NamedSinksTest, NamedAndUnnamedMixed) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("test_unnamed.txt"); // auto: sink_0
    logger.addSink<minta::FileSink>("named", "test_named_mix.txt");
    logger.addSink<minta::FileSink>("test_unnamed2.txt"); // auto: sink_2
    // All three should receive logs
    logger.info("Mixed sinks");
    logger.flush();
    TestUtils::waitForFileContent("test_unnamed.txt");
    TestUtils::waitForFileContent("test_named_mix.txt");
    TestUtils::waitForFileContent("test_unnamed2.txt");
    std::string c1 = TestUtils::readLogFile("test_unnamed.txt");
    std::string c2 = TestUtils::readLogFile("test_named_mix.txt");
    std::string c3 = TestUtils::readLogFile("test_unnamed2.txt");
    EXPECT_TRUE(c1.find("Mixed sinks") != std::string::npos);
    EXPECT_TRUE(c2.find("Mixed sinks") != std::string::npos);
    EXPECT_TRUE(c3.find("Mixed sinks") != std::string::npos);
}

TEST_F(NamedSinksTest, SinkProxyLocale) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("loc", "test_locale.txt");
    logger.sink("loc").locale("en_US.UTF-8");
    logger.info("Locale test");
    logger.flush();
    TestUtils::waitForFileContent("test_locale.txt");
    std::string content = TestUtils::readLogFile("test_locale.txt");
    EXPECT_TRUE(content.find("Locale test") != std::string::npos);
}

TEST_F(NamedSinksTest, EmptyNameThrowsOnDuplicate) {
    // Empty string is a valid name
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("", "test_empty_name.txt");
    EXPECT_THROW(logger.addSink<minta::FileSink>("", "test_empty_name2.txt"), std::invalid_argument);
}

TEST_F(NamedSinksTest, AddSinkAfterLoggingThrows) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("pre", "test_pre.txt");
    logger.info("Start logging");
    logger.flush();
    // After logging starts, adding sinks should throw
    EXPECT_THROW(logger.addSink<minta::FileSink>("post", "test_post.txt"), std::logic_error);
}

TEST_F(NamedSinksTest, SinkProxyFormatterChange) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("fmt", "test_fmt.txt");
    logger.sink("fmt").formatter(minta::detail::make_unique<minta::JsonFormatter>());
    logger.info("JSON format");
    logger.flush();
    TestUtils::waitForFileContent("test_fmt.txt");
    std::string content = TestUtils::readLogFile("test_fmt.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
}

TEST_F(NamedSinksTest, MultipleProxyCallsOnSameSink) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("multi", "test_multi_proxy.txt");
    logger.sink("multi").level(minta::LogLevel::WARN).filterRule("level >= WARN");
    logger.trace("Trace msg");
    logger.warn("Warn msg");
    logger.flush();
    TestUtils::waitForFileContent("test_multi_proxy.txt");
    std::string content = TestUtils::readLogFile("test_multi_proxy.txt");
    EXPECT_TRUE(content.find("Trace msg") == std::string::npos);
    EXPECT_TRUE(content.find("Warn msg") != std::string::npos);
}

TEST_F(NamedSinksTest, AddSinkConsoleSinkNamed) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::ConsoleSink>("console");
    logger.sink("console").level(minta::LogLevel::ERROR);
    SUCCEED();
}

TEST_F(NamedSinksTest, AddCustomSinkNamed) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::ConsoleSink>();
    logger.addCustomSink("custom", std::move(sink));
    logger.sink("custom").level(minta::LogLevel::WARN);
    SUCCEED();
}

TEST_F(NamedSinksTest, NamedSinkJsonFormatterSpecExample) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("json-out", "test_spec_json.txt");
    logger.info("Spec example {val}", 42);
    logger.flush();
    TestUtils::waitForFileContent("test_spec_json.txt");
    std::string content = TestUtils::readLogFile("test_spec_json.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
}

TEST_F(NamedSinksTest, AutoNameCollisionSkips) {
    // H1/M10: User names a sink "sink_0", then adds an unnamed sink.
    // Auto-naming should skip "sink_0" and use "sink_1" (or next free).
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("sink_0", "test_collision_named.txt");
    logger.addSink<minta::FileSink>("test_collision_auto.txt"); // should get "sink_1", not "sink_0"
    // Verify auto-named sink is accessible and didn't collide
    logger.sink("sink_1").level(minta::LogLevel::ERROR);
    logger.info("Info msg");
    logger.error("Error msg");
    logger.flush();
    TestUtils::waitForFileContent("test_collision_named.txt");
    TestUtils::waitForFileContent("test_collision_auto.txt");
    std::string namedContent = TestUtils::readLogFile("test_collision_named.txt");
    std::string autoContent = TestUtils::readLogFile("test_collision_auto.txt");
    // Named sink (sink_0) should have both messages (default level TRACE)
    EXPECT_TRUE(namedContent.find("Info msg") != std::string::npos);
    EXPECT_TRUE(namedContent.find("Error msg") != std::string::npos);
    // Auto sink (sink_1) should only have error (level set to ERROR)
    EXPECT_TRUE(autoContent.find("Info msg") == std::string::npos);
    EXPECT_TRUE(autoContent.find("Error msg") != std::string::npos);
}

TEST_F(NamedSinksTest, SinkProxyFormatterThrowsAfterLogging) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("fmt_throw", "test_fmt_throw.txt");
    logger.info("Start logging to trigger m_loggingStarted");
    logger.flush();
    // formatter() should throw after logging has started
    EXPECT_THROW(
        logger.sink("fmt_throw").formatter(
            minta::detail::make_unique<minta::JsonFormatter>()),
        std::logic_error);
}

TEST_F(NamedSinksTest, SinkProxyFilterPredicate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("pred", "test_pred_filter.txt");
    // Set a predicate filter that only allows ERROR+
    logger.sink("pred").filter([](const minta::LogEntry& entry) {
        return entry.level >= minta::LogLevel::ERROR;
    });
    logger.info("Should be filtered out");
    logger.error("Should pass through");
    logger.flush();
    TestUtils::waitForFileContent("test_pred_filter.txt");
    std::string content = TestUtils::readLogFile("test_pred_filter.txt");
    EXPECT_TRUE(content.find("Should be filtered out") == std::string::npos);
    EXPECT_TRUE(content.find("Should pass through") != std::string::npos);
}

TEST_F(NamedSinksTest, ThreeSinksWithDifferentLevels) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("trace", "test_trace.txt");
    logger.addSink<minta::FileSink>("info", "test_info_lvl.txt");
    logger.addSink<minta::FileSink>("error", "test_error_lvl.txt");
    logger.sink("info").level(minta::LogLevel::INFO);
    logger.sink("error").level(minta::LogLevel::ERROR);
    
    logger.trace("Trace msg");
    logger.info("Info msg");
    logger.error("Error msg");
    logger.flush();
    
    TestUtils::waitForFileContent("test_trace.txt");
    TestUtils::waitForFileContent("test_info_lvl.txt");
    TestUtils::waitForFileContent("test_error_lvl.txt");
    
    std::string traceContent = TestUtils::readLogFile("test_trace.txt");
    std::string infoContent = TestUtils::readLogFile("test_info_lvl.txt");
    std::string errorContent = TestUtils::readLogFile("test_error_lvl.txt");
    
    EXPECT_TRUE(traceContent.find("Trace msg") != std::string::npos);
    EXPECT_TRUE(traceContent.find("Info msg") != std::string::npos);
    EXPECT_TRUE(traceContent.find("Error msg") != std::string::npos);
    
    EXPECT_TRUE(infoContent.find("Trace msg") == std::string::npos);
    EXPECT_TRUE(infoContent.find("Info msg") != std::string::npos);
    EXPECT_TRUE(infoContent.find("Error msg") != std::string::npos);
    
    EXPECT_TRUE(errorContent.find("Trace msg") == std::string::npos);
    EXPECT_TRUE(errorContent.find("Info msg") == std::string::npos);
    EXPECT_TRUE(errorContent.find("Error msg") != std::string::npos);
}
