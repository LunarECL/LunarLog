#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <chrono>
#ifndef _WIN32
#include <sys/stat.h>
#endif

class DynamicConfigTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }

    /// Write a JSON config file for tests.
    static void writeConfigFile(const std::string& path,
                                const std::string& content) {
        std::ofstream ofs(path.c_str());
        ofs << content;
        ofs.flush();
    }

    /// Rewrite a config file with a delay that guarantees mtime changes.
    /// Many filesystems (ext3, HFS+, FAT32) store mtime with 1-second
    /// granularity, so a sub-second rewrite may produce the same mtime.
    /// The 1100ms sleep ensures the new write gets a strictly later mtime
    /// that the polling watcher will detect.
    static void touchFile(const std::string& path,
                          const std::string& content) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        std::ofstream ofs(path.c_str());
        ofs << content;
        ofs.flush();
    }
};

// ---- JSON Parser Unit Tests ----

TEST_F(DynamicConfigTest, JsonParserNull) {
    auto v = minta::detail::JsonValue::parse("null");
    EXPECT_TRUE(v.isNull());
}

TEST_F(DynamicConfigTest, JsonParserBool) {
    auto t = minta::detail::JsonValue::parse("true");
    EXPECT_TRUE(t.isBool());
    EXPECT_TRUE(t.asBool());

    auto f = minta::detail::JsonValue::parse("false");
    EXPECT_TRUE(f.isBool());
    EXPECT_FALSE(f.asBool());
}

TEST_F(DynamicConfigTest, JsonParserNumber) {
    auto v = minta::detail::JsonValue::parse("42");
    EXPECT_TRUE(v.isNumber());
    EXPECT_DOUBLE_EQ(v.asNumber(), 42.0);

    auto neg = minta::detail::JsonValue::parse("-3.14");
    EXPECT_DOUBLE_EQ(neg.asNumber(), -3.14);

    auto exp = minta::detail::JsonValue::parse("1e3");
    EXPECT_DOUBLE_EQ(exp.asNumber(), 1000.0);
}

TEST_F(DynamicConfigTest, JsonParserString) {
    auto v = minta::detail::JsonValue::parse("\"hello world\"");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString(), "hello world");
}

TEST_F(DynamicConfigTest, JsonParserStringEscapes) {
    auto v = minta::detail::JsonValue::parse("\"tab\\there\\nnewline\"");
    EXPECT_EQ(v.asString(), "tab\there\nnewline");

    auto u = minta::detail::JsonValue::parse("\"\\u0041\"");
    EXPECT_EQ(u.asString(), "A");
}

TEST_F(DynamicConfigTest, JsonParserArray) {
    auto v = minta::detail::JsonValue::parse("[1, \"two\", true, null]");
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.asArray().size(), 4u);
    EXPECT_DOUBLE_EQ(v.asArray()[0].asNumber(), 1.0);
    EXPECT_EQ(v.asArray()[1].asString(), "two");
    EXPECT_TRUE(v.asArray()[2].asBool());
    EXPECT_TRUE(v.asArray()[3].isNull());
}

TEST_F(DynamicConfigTest, JsonParserObject) {
    auto v = minta::detail::JsonValue::parse(
        "{\"name\": \"LunarLog\", \"version\": 1}");
    EXPECT_TRUE(v.isObject());
    EXPECT_TRUE(v.hasKey("name"));
    EXPECT_EQ(v["name"].asString(), "LunarLog");
    EXPECT_DOUBLE_EQ(v["version"].asNumber(), 1.0);
    EXPECT_FALSE(v.hasKey("missing"));
    EXPECT_TRUE(v["missing"].isNull());
}

TEST_F(DynamicConfigTest, JsonParserEmptyObject) {
    auto v = minta::detail::JsonValue::parse("{}");
    EXPECT_TRUE(v.isObject());
    EXPECT_TRUE(v.asObject().empty());
}

TEST_F(DynamicConfigTest, JsonParserEmptyArray) {
    auto v = minta::detail::JsonValue::parse("[]");
    EXPECT_TRUE(v.isArray());
    EXPECT_TRUE(v.asArray().empty());
}

TEST_F(DynamicConfigTest, JsonParserNestedObjects) {
    auto v = minta::detail::JsonValue::parse(
        "{\"sinks\": {\"console\": {\"level\": \"WARN\"}}}");
    EXPECT_TRUE(v["sinks"].isObject());
    EXPECT_TRUE(v["sinks"]["console"].isObject());
    EXPECT_EQ(v["sinks"]["console"]["level"].asString(), "WARN");
}

TEST_F(DynamicConfigTest, JsonParserInvalidThrows) {
    EXPECT_THROW(minta::detail::JsonValue::parse(""), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("{invalid}"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("[1, 2,]"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"unterminated"), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserTrailingContentThrows) {
    EXPECT_THROW(minta::detail::JsonValue::parse("42 extra"), std::runtime_error);
}

// ---- LevelSwitch Unit Tests ----

TEST_F(DynamicConfigTest, LevelSwitchBasic) {
    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::INFO);
    EXPECT_EQ(sw->get(), minta::LogLevel::INFO);
    sw->set(minta::LogLevel::DEBUG);
    EXPECT_EQ(sw->get(), minta::LogLevel::DEBUG);
    sw->set(minta::LogLevel::FATAL);
    EXPECT_EQ(sw->get(), minta::LogLevel::FATAL);
}

TEST_F(DynamicConfigTest, LevelSwitchDefaultLevel) {
    auto sw = std::make_shared<minta::LevelSwitch>();
    EXPECT_EQ(sw->get(), minta::LogLevel::INFO);
}

TEST_F(DynamicConfigTest, LevelSwitchImmediateEffect) {
    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::WARN);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .writeTo<minta::FileSink>("dc_level_switch.txt")
        .build();

    logger.debug("Debug before switch");
    logger.info("Info before switch");
    logger.warn("Warn before switch");
    logger.flush();

    // Change level to DEBUG — should take effect immediately
    sw->set(minta::LogLevel::DEBUG);

    logger.debug("Debug after switch");
    logger.info("Info after switch");
    logger.flush();

    TestUtils::waitForFileContent("dc_level_switch.txt");
    std::string content = TestUtils::readLogFile("dc_level_switch.txt");

    EXPECT_EQ(content.find("Debug before switch"), std::string::npos);
    EXPECT_EQ(content.find("Info before switch"), std::string::npos);
    EXPECT_NE(content.find("Warn before switch"), std::string::npos);
    EXPECT_NE(content.find("Debug after switch"), std::string::npos);
    EXPECT_NE(content.find("Info after switch"), std::string::npos);
}

TEST_F(DynamicConfigTest, LevelSwitchThreadSafety) {
    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::INFO);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .writeTo<minta::FileSink>("dc_level_thread.txt")
        .build();

    std::atomic<bool> done(false);
    const int iterations = 200;

    std::thread levelChanger([&]() {
        for (int i = 0; i < iterations && !done.load(); ++i) {
            sw->set(minta::LogLevel::DEBUG);
            sw->set(minta::LogLevel::WARN);
            sw->set(minta::LogLevel::ERROR);
            sw->set(minta::LogLevel::TRACE);
        }
    });

    std::thread logWriter([&]() {
        for (int i = 0; i < iterations * 5 && !done.load(); ++i) {
            logger.info("Message {n}", i);
        }
    });

    logWriter.join();
    done.store(true);
    levelChanger.join();
    logger.flush();

    // No crash = success
    SUCCEED();
}

TEST_F(DynamicConfigTest, LevelSwitchSharedBetweenLoggers) {
    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::WARN);

    auto log1 = minta::LunarLog::configure()
        .minLevel(sw)
        .writeTo<minta::FileSink>("dc_shared_1.txt")
        .build();

    auto log2 = minta::LunarLog::configure()
        .minLevel(sw)
        .writeTo<minta::FileSink>("dc_shared_2.txt")
        .build();

    log1.info("Logger1 info before");
    log2.info("Logger2 info before");
    log1.flush();
    log2.flush();

    // Change level — affects both loggers
    sw->set(minta::LogLevel::DEBUG);

    log1.info("Logger1 info after");
    log2.info("Logger2 info after");
    log1.flush();
    log2.flush();

    TestUtils::waitForFileContent("dc_shared_1.txt");
    TestUtils::waitForFileContent("dc_shared_2.txt");
    std::string c1 = TestUtils::readLogFile("dc_shared_1.txt");
    std::string c2 = TestUtils::readLogFile("dc_shared_2.txt");

    EXPECT_EQ(c1.find("Logger1 info before"), std::string::npos);
    EXPECT_NE(c1.find("Logger1 info after"), std::string::npos);
    EXPECT_EQ(c2.find("Logger2 info before"), std::string::npos);
    EXPECT_NE(c2.find("Logger2 info after"), std::string::npos);
}

TEST_F(DynamicConfigTest, LevelSwitchWithImperativeAPI) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("dc_imperative.txt");

    // Verify basic level works
    logger.debug("Debug before");
    logger.info("Info before");
    logger.flush();

    // Change level via setMinLevel (imperative)
    logger.setMinLevel(minta::LogLevel::DEBUG);
    logger.debug("Debug after");
    logger.flush();

    TestUtils::waitForFileContent("dc_imperative.txt");
    std::string content = TestUtils::readLogFile("dc_imperative.txt");

    EXPECT_EQ(content.find("Debug before"), std::string::npos);
    EXPECT_NE(content.find("Info before"), std::string::npos);
    EXPECT_NE(content.find("Debug after"), std::string::npos);
}

// ---- tryParseLogLevel tests ----

TEST_F(DynamicConfigTest, TryParseLogLevelValid) {
    minta::LogLevel lvl;
    EXPECT_TRUE(minta::detail::tryParseLogLevel("TRACE", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::TRACE);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("DEBUG", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::DEBUG);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("INFO", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::INFO);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("WARN", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::WARN);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("ERROR", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::ERROR);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("FATAL", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::FATAL);
}

TEST_F(DynamicConfigTest, TryParseLogLevelInvalid) {
    minta::LogLevel lvl = minta::LogLevel::INFO;
    EXPECT_FALSE(minta::detail::tryParseLogLevel("INVALID", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::INFO); // unchanged
    EXPECT_FALSE(minta::detail::tryParseLogLevel("", lvl));
    EXPECT_FALSE(minta::detail::tryParseLogLevel("WARNING", lvl));
}

// ---- Config File Watcher Integration Tests ----

TEST_F(DynamicConfigTest, WatchConfigMinLevel) {
    const std::string configPath = "dc_test_config.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"WARN\" }");

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::WARN);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_watch_level.txt")
        .build();

    // Trigger watcher start
    logger.warn("Initial warn");
    logger.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    logger.debug("Debug blocked");
    logger.warn("Warn passes");
    logger.flush();

    touchFile(configPath, "{ \"minLevel\": \"DEBUG\" }");

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    logger.debug("Debug now passes");
    logger.flush();

    TestUtils::waitForFileContent("dc_watch_level.txt");
    std::string content = TestUtils::readLogFile("dc_watch_level.txt");

    EXPECT_NE(content.find("Initial warn"), std::string::npos);
    EXPECT_EQ(content.find("Debug blocked"), std::string::npos);
    EXPECT_NE(content.find("Warn passes"), std::string::npos);
    EXPECT_NE(content.find("Debug now passes"), std::string::npos);

    // Cleanup config file
    std::remove(configPath.c_str());
}

TEST_F(DynamicConfigTest, WatchConfigMalformedKeepsCurrentSettings) {
    const std::string configPath = "dc_test_malformed.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"DEBUG\" }");

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::DEBUG);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_watch_malformed.txt")
        .build();

    // Trigger watcher start
    logger.debug("Debug initially");
    logger.flush();

    // Wait for initial load
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Write malformed JSON
    touchFile(configPath, "{ this is not valid JSON }");

    // Wait for reload attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Should still be at DEBUG level (graceful degradation)
    logger.debug("Debug still works");
    logger.flush();

    TestUtils::waitForFileContent("dc_watch_malformed.txt");
    std::string content = TestUtils::readLogFile("dc_watch_malformed.txt");

    EXPECT_NE(content.find("Debug initially"), std::string::npos);
    EXPECT_NE(content.find("Debug still works"), std::string::npos);

    std::remove(configPath.c_str());
}

TEST_F(DynamicConfigTest, WatchConfigPerSinkLevel) {
    const std::string configPath = "dc_test_sinks.json";
    writeConfigFile(configPath,
        "{ \"sinks\": { \"errors\": { \"level\": \"ERROR\" } } }");

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("errors", "dc_sink_errors.txt")
        .writeTo<minta::FileSink>("all", "dc_sink_all.txt")
        .build();

    // Trigger watcher start
    logger.info("Trigger watcher");
    logger.flush();

    // Wait for initial config load
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    logger.info("Info message");
    logger.error("Error message");
    logger.flush();

    TestUtils::waitForFileContent("dc_sink_all.txt");
    std::string all = TestUtils::readLogFile("dc_sink_all.txt");
    std::string errors = TestUtils::readLogFile("dc_sink_errors.txt");

    // "all" sink should have both
    EXPECT_NE(all.find("Info message"), std::string::npos);
    EXPECT_NE(all.find("Error message"), std::string::npos);

    // "errors" sink should only have ERROR+
    EXPECT_EQ(errors.find("Info message"), std::string::npos);
    EXPECT_NE(errors.find("Error message"), std::string::npos);

    std::remove(configPath.c_str());
}

TEST_F(DynamicConfigTest, WatchConfigFilterHotReload) {
    const std::string configPath = "dc_test_filters.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"TRACE\", \"filters\": [] }");

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_filter_reload.txt")
        .build();

    logger.info("Trigger watcher");
    logger.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    logger.info("heartbeat ping");
    logger.info("Important event");
    logger.flush();

    touchFile(configPath,
        "{ \"minLevel\": \"TRACE\", \"filters\": [\"INFO+ !~heartbeat\"] }");

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    logger.info("heartbeat blocked");
    logger.info("Another important event");
    logger.flush();

    TestUtils::waitForFileContent("dc_filter_reload.txt");
    std::string content = TestUtils::readLogFile("dc_filter_reload.txt");

    // Before filter: both pass
    EXPECT_NE(content.find("heartbeat ping"), std::string::npos);
    EXPECT_NE(content.find("Important event"), std::string::npos);
    // After filter: heartbeat blocked
    EXPECT_EQ(content.find("heartbeat blocked"), std::string::npos);
    EXPECT_NE(content.find("Another important event"), std::string::npos);
}

TEST_F(DynamicConfigTest, WatchConfigMissingFileDoesNotCrash) {
    // Config file does not exist
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::INFO)
        .watchConfig("dc_nonexistent_config.json", std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_missing_file.txt")
        .build();

    // Trigger watcher — should handle missing file gracefully
    logger.info("Works without config file");
    logger.flush();

    TestUtils::waitForFileContent("dc_missing_file.txt");
    std::string content = TestUtils::readLogFile("dc_missing_file.txt");

    EXPECT_NE(content.find("Works without config file"), std::string::npos);
}

TEST_F(DynamicConfigTest, WatchConfigUnchangedFileNotReloaded) {
    const std::string configPath = "dc_test_nochange.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"WARN\" }");

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::WARN);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_nochange.txt")
        .build();

    // Trigger watcher
    logger.warn("Initial warn");
    logger.flush();

    // Wait for initial config load + one poll cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    // Level should still be WARN (file hasn't changed)
    EXPECT_EQ(sw->get(), minta::LogLevel::WARN);

    logger.info("Info blocked");
    logger.warn("Warn still passes");
    logger.flush();

    TestUtils::waitForFileContent("dc_nochange.txt");
    std::string content = TestUtils::readLogFile("dc_nochange.txt");

    EXPECT_NE(content.find("Initial warn"), std::string::npos);
    EXPECT_EQ(content.find("Info blocked"), std::string::npos);
    EXPECT_NE(content.find("Warn still passes"), std::string::npos);

    std::remove(configPath.c_str());
}

// ---- Builder API Integration Tests ----

TEST_F(DynamicConfigTest, BuilderWithLevelSwitchAndConfig) {
    const std::string configPath = "dc_test_full.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"INFO\", "
        "\"sinks\": { \"app\": { \"level\": \"DEBUG\" } }, "
        "\"filters\": [] }");

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::INFO);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("app", "dc_builder_full.txt")
        .build();

    logger.info("Builder info message");
    logger.flush();

    TestUtils::waitForFileContent("dc_builder_full.txt");
    std::string content = TestUtils::readLogFile("dc_builder_full.txt");

    EXPECT_NE(content.find("Builder info message"), std::string::npos);

    std::remove(configPath.c_str());
}

TEST_F(DynamicConfigTest, LevelSwitchRestrictsLowerThanSetMinLevel) {
    // LevelSwitch starts at ERROR, so even though builder says DEBUG,
    // the LevelSwitch overrides
    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::ERROR);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .writeTo<minta::FileSink>("dc_switch_restrict.txt")
        .build();

    logger.info("Info blocked by switch");
    logger.warn("Warn blocked by switch");
    logger.error("Error passes");
    logger.flush();

    TestUtils::waitForFileContent("dc_switch_restrict.txt");
    std::string content = TestUtils::readLogFile("dc_switch_restrict.txt");

    EXPECT_EQ(content.find("Info blocked by switch"), std::string::npos);
    EXPECT_EQ(content.find("Warn blocked by switch"), std::string::npos);
    EXPECT_NE(content.find("Error passes"), std::string::npos);
}

// ---- Config file format validation ----

TEST_F(DynamicConfigTest, ConfigWithFullFormat) {
    // Test that the full config format parses correctly
    std::string json =
        "{\n"
        "    \"minLevel\": \"DEBUG\",\n"
        "    \"sinks\": {\n"
        "        \"console\": { \"level\": \"WARN\" },\n"
        "        \"file\": { \"level\": \"DEBUG\" }\n"
        "    },\n"
        "    \"filters\": [\"INFO+ !~heartbeat\"]\n"
        "}";

    auto config = minta::detail::JsonValue::parse(json);
    EXPECT_TRUE(config.isObject());
    EXPECT_EQ(config["minLevel"].asString(), "DEBUG");
    EXPECT_TRUE(config["sinks"].isObject());
    EXPECT_EQ(config["sinks"]["console"]["level"].asString(), "WARN");
    EXPECT_EQ(config["sinks"]["file"]["level"].asString(), "DEBUG");
    EXPECT_TRUE(config["filters"].isArray());
    EXPECT_EQ(config["filters"].asArray().size(), 1u);
    EXPECT_EQ(config["filters"].asArray()[0].asString(), "INFO+ !~heartbeat");
}

TEST_F(DynamicConfigTest, ConfigWithMultipleFilters) {
    std::string json =
        "{ \"filters\": [\"WARN+\", \"~heartbeat\", \"INFO+ ~debug\"] }";
    auto config = minta::detail::JsonValue::parse(json);
    EXPECT_EQ(config["filters"].asArray().size(), 3u);
}

// ---- Additional JSON Parser Edge Case Tests ----

TEST_F(DynamicConfigTest, JsonParserWhitespaceOnlyThrows) {
    EXPECT_THROW(minta::detail::JsonValue::parse("   "), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\n\t\r "), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserUnescapedControlCharThrows) {
    std::string rawTab = std::string("\"hello") + '\x09' + "world\"";
    EXPECT_THROW(minta::detail::JsonValue::parse(rawTab), std::runtime_error);
    std::string rawNl = std::string("\"line1") + '\x0A' + "line2\"";
    EXPECT_THROW(minta::detail::JsonValue::parse(rawNl), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserEscapedControlCharOk) {
    auto v = minta::detail::JsonValue::parse("\"tab\\there\"");
    EXPECT_EQ(v.asString(), "tab\there");
    auto n = minta::detail::JsonValue::parse("\"newline\\nhere\"");
    EXPECT_EQ(n.asString(), "newline\nhere");
}

TEST_F(DynamicConfigTest, JsonParserLoneSurrogateThrows) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uD800\""),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uDC00\""),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uD800\\u0041\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserValidSurrogatePair) {
    auto v = minta::detail::JsonValue::parse("\"\\uD83D\\uDE00\"");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString().size(), 4u);
}

TEST_F(DynamicConfigTest, JsonParserDeepNestingThrows) {
    std::string deep(200, '[');
    deep += "null";
    deep += std::string(200, ']');
    EXPECT_THROW(minta::detail::JsonValue::parse(deep), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserNestingWithinLimit) {
    std::string nested(50, '[');
    nested += "null";
    nested += std::string(50, ']');
    auto v = minta::detail::JsonValue::parse(nested);
    EXPECT_TRUE(v.isArray());
}

// ---- Case-insensitive log level parsing ----

TEST_F(DynamicConfigTest, TryParseLogLevelCaseInsensitive) {
    minta::LogLevel lvl;
    EXPECT_TRUE(minta::detail::tryParseLogLevel("info", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::INFO);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("Debug", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::DEBUG);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("warn", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::WARN);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("trace", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::TRACE);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("Error", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::ERROR);
    EXPECT_TRUE(minta::detail::tryParseLogLevel("fAtAl", lvl));
    EXPECT_EQ(lvl, minta::LogLevel::FATAL);
}

// ---- watchConfig without LevelSwitch (tests setMinLevel fallback) ----

TEST_F(DynamicConfigTest, WatchConfigWithoutLevelSwitch) {
    const std::string configPath = "dc_test_nolevelswitch.json";
    writeConfigFile(configPath, "{ \"minLevel\": \"WARN\" }");

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::WARN)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_nolevelswitch.txt")
        .build();

    logger.warn("Initial warn");
    logger.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    logger.info("Info blocked");
    logger.warn("Warn passes");
    logger.flush();

    touchFile(configPath, "{ \"minLevel\": \"DEBUG\" }");
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    logger.debug("Debug now passes");
    logger.flush();

    TestUtils::waitForFileContent("dc_nolevelswitch.txt");
    std::string content = TestUtils::readLogFile("dc_nolevelswitch.txt");

    EXPECT_NE(content.find("Initial warn"), std::string::npos);
    EXPECT_EQ(content.find("Info blocked"), std::string::npos);
    EXPECT_NE(content.find("Warn passes"), std::string::npos);
    EXPECT_NE(content.find("Debug now passes"), std::string::npos);

    std::remove(configPath.c_str());
}

// ---- Concurrent config reload stress test ----
// Crash-safety test: verifies no data races, deadlocks, or undefined
// behavior under concurrent logging + config file changes.  Does NOT
// assert reload counts — the 100ms write interval may not change mtime
// on filesystems with 1-second granularity (HFS+, ext3, FAT32).

TEST_F(DynamicConfigTest, WatchConfigConcurrentStress) {
    const std::string configPath = "dc_test_concurrent.json";
    writeConfigFile(configPath, "{ \"minLevel\": \"INFO\" }");

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::INFO);

    auto logger = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_concurrent.txt")
        .build();

    logger.info("Initial stress test message");
    logger.flush();

    std::atomic<bool> done(false);

    std::vector<std::thread> writers;
    for (int w = 0; w < 4; ++w) {
        writers.emplace_back([&logger, &done, w]() {
            for (int i = 0; i < 200 &&
                 !done.load(std::memory_order_relaxed); ++i) {
                logger.info("Writer {w} message {i}", "w", w, "i", i);
            }
        });
    }

    std::thread configChanger([&configPath, &done]() {
        const char* levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        for (int i = 0; i < 5 &&
             !done.load(std::memory_order_relaxed); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::ofstream ofs(configPath.c_str());
            ofs << "{ \"minLevel\": \"" << levels[i % 4] << "\" }";
            ofs.flush();
        }
    });

    for (auto& t : writers) t.join();
    done.store(true, std::memory_order_relaxed);
    configChanger.join();
    logger.flush();

    SUCCEED();

    std::remove(configPath.c_str());
}

TEST_F(DynamicConfigTest, JsonParserPeekEndOfInput) {
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\":1,"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("[1,"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserExpectMismatch) {
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\" }"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\" ]"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserUnterminatedEscapeSeq) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"hello\\"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserAllEscapeChars) {
    auto q = minta::detail::JsonValue::parse("\"say \\\"hi\\\"\"");
    EXPECT_EQ(q.asString(), "say \"hi\"");

    auto bs = minta::detail::JsonValue::parse("\"a\\\\b\"");
    EXPECT_EQ(bs.asString(), "a\\b");

    auto fs = minta::detail::JsonValue::parse("\"a\\/b\"");
    EXPECT_EQ(fs.asString(), "a/b");

    auto b = minta::detail::JsonValue::parse("\"x\\by\"");
    EXPECT_EQ(b.asString(), std::string("x\by"));

    auto f = minta::detail::JsonValue::parse("\"x\\fy\"");
    EXPECT_EQ(f.asString(), std::string("x\fy"));

    auto r = minta::detail::JsonValue::parse("\"x\\ry\"");
    EXPECT_EQ(r.asString(), std::string("x\ry"));
}

TEST_F(DynamicConfigTest, JsonParserInvalidEscapeChar) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"test\\x\""),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"test\\a\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserUnicode2ByteUtf8) {
    auto v = minta::detail::JsonValue::parse("\"\\u00E9\"");
    EXPECT_EQ(v.asString(), "\xC3\xA9");
    EXPECT_EQ(v.asString().size(), 2u);

    auto v2 = minta::detail::JsonValue::parse("\"\\u00e9\"");
    EXPECT_EQ(v2.asString(), "\xC3\xA9");
}

TEST_F(DynamicConfigTest, JsonParserUnicode3ByteUtf8) {
    auto v = minta::detail::JsonValue::parse("\"\\u4E16\"");
    EXPECT_EQ(v.asString(), "\xE4\xB8\x96");
    EXPECT_EQ(v.asString().size(), 3u);
}

TEST_F(DynamicConfigTest, JsonParserUnicode4ByteSurrogatePair) {
    auto v = minta::detail::JsonValue::parse("\"\\uD834\\uDD1E\"");
    EXPECT_EQ(v.asString(), "\xF0\x9D\x84\x9E");
    EXPECT_EQ(v.asString().size(), 4u);

    auto v2 = minta::detail::JsonValue::parse("\"\\uD834\\udd1e\"");
    EXPECT_EQ(v2.asString(), "\xF0\x9D\x84\x9E");
}

TEST_F(DynamicConfigTest, JsonParserInvalidHexInUnicodeEscape) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uXXXX\""),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\u00GZ\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserIncompleteUnicodeEscape) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\u00\""),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\u\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserInvalidHexInSurrogatePairLow) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uD800\\uZZZZ\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserHighSurrogateFollowedByNonLowAbove) {
    EXPECT_THROW(minta::detail::JsonValue::parse("\"\\uD800\\uE000\""),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserNumberLeadingZeroDecimal) {
    auto v = minta::detail::JsonValue::parse("0.5");
    EXPECT_DOUBLE_EQ(v.asNumber(), 0.5);

    auto v2 = minta::detail::JsonValue::parse("0.123");
    EXPECT_DOUBLE_EQ(v2.asNumber(), 0.123);
}

TEST_F(DynamicConfigTest, JsonParserNumberExponentWithSign) {
    auto v = minta::detail::JsonValue::parse("1e+2");
    EXPECT_DOUBLE_EQ(v.asNumber(), 100.0);

    auto v2 = minta::detail::JsonValue::parse("1.5E-3");
    EXPECT_DOUBLE_EQ(v2.asNumber(), 0.0015);

    auto v3 = minta::detail::JsonValue::parse("-0.5E-3");
    EXPECT_DOUBLE_EQ(v3.asNumber(), -0.0005);

    auto v4 = minta::detail::JsonValue::parse("1.5e10");
    EXPECT_DOUBLE_EQ(v4.asNumber(), 1.5e10);
}

TEST_F(DynamicConfigTest, JsonParserUnexpectedCharAsValue) {
    EXPECT_THROW(minta::detail::JsonValue::parse("&"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("%"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("@"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserDuplicateKeys) {
    auto v = minta::detail::JsonValue::parse("{\"a\":1,\"a\":2}");
    EXPECT_TRUE(v.isObject());
    EXPECT_DOUBLE_EQ(v["a"].asNumber(), 2.0);
}

TEST_F(DynamicConfigTest, JsonParserTruncatedLiterals) {
    EXPECT_THROW(minta::detail::JsonValue::parse("tru"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("fals"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("nul"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserMismatchedLiteral) {
    EXPECT_THROW(minta::detail::JsonValue::parse("trux"),
                 std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("falsx"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserObjectMissingComma) {
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\":1 \"b\":2}"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserArrayMissingComma) {
    EXPECT_THROW(minta::detail::JsonValue::parse("[1 2]"),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserObjectDepthLimit) {
    std::string deep;
    for (int i = 0; i < 200; ++i) deep += "{\"a\":";
    deep += "null";
    for (int i = 0; i < 200; ++i) deep += "}";
    EXPECT_THROW(minta::detail::JsonValue::parse(deep),
                 std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserMixedNestingWithinLimit) {
    std::string nested;
    for (int i = 0; i < 50; ++i) nested += "[{\"v\":";
    nested += "null";
    for (int i = 0; i < 50; ++i) nested += "}]";
    auto v = minta::detail::JsonValue::parse(nested);
    EXPECT_TRUE(v.isArray());
}

TEST_F(DynamicConfigTest, ConfigWatcherDoubleStart) {
    const std::string configPath = "dc_test_doublestart.json";
    writeConfigFile(configPath, "{ \"minLevel\": \"DEBUG\" }");

    int loadCount = 0;
    minta::detail::ConfigWatcher watcher(
        configPath,
        std::chrono::seconds(60),
        [&loadCount](const minta::detail::JsonValue&) { ++loadCount; },
        [](const std::string&) {}
    );

    watcher.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    watcher.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    watcher.stop();

    EXPECT_EQ(loadCount, 1);

    std::remove(configPath.c_str());
}

#ifndef _WIN32
TEST_F(DynamicConfigTest, ConfigWatcherFileUnreadable) {
    const std::string configPath = "dc_test_unreadable.json";
    writeConfigFile(configPath, "{ \"minLevel\": \"DEBUG\" }");

    chmod(configPath.c_str(), 0000);

    std::string warningMsg;
    bool configLoaded = false;

    minta::detail::ConfigWatcher watcher(
        configPath,
        std::chrono::seconds(60),
        [&configLoaded](const minta::detail::JsonValue&) {
            configLoaded = true;
        },
        [&warningMsg](const std::string& msg) { warningMsg = msg; }
    );

    watcher.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    watcher.stop();

    chmod(configPath.c_str(), 0644);
    std::remove(configPath.c_str());

    EXPECT_FALSE(configLoaded);
    EXPECT_FALSE(warningMsg.empty());
    EXPECT_NE(warningMsg.find("cannot open"), std::string::npos);
}
#endif

TEST_F(DynamicConfigTest, ConfigAllInvalidFiltersPreserveExisting) {
    const std::string configPath = "dc_test_badfilters.json";
    writeConfigFile(configPath,
        "{ \"minLevel\": \"TRACE\", \"filters\": [\"WARN+ !~heartbeat\"] }");

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .watchConfig(configPath, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("dc_badfilters.txt")
        .build();

    logger.info("Trigger watcher");
    logger.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    logger.warn("heartbeat ping before");
    logger.warn("Important event before");
    logger.flush();

    touchFile(configPath,
        "{ \"minLevel\": \"TRACE\", \"filters\": [\"@@@\", \"$$$\"] }");

    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    logger.warn("heartbeat ping after");
    logger.warn("Important event after");
    logger.flush();

    TestUtils::waitForFileContent("dc_badfilters.txt");
    std::string content = TestUtils::readLogFile("dc_badfilters.txt");

    EXPECT_EQ(content.find("heartbeat ping before"), std::string::npos);
    EXPECT_NE(content.find("Important event before"), std::string::npos);

    EXPECT_EQ(content.find("heartbeat ping after"), std::string::npos);
    EXPECT_NE(content.find("Important event after"), std::string::npos);

    std::remove(configPath.c_str());
}

// --- Additional coverage tests for uncovered error paths ---

TEST_F(DynamicConfigTest, JsonParserAdvancePastEnd) {
    EXPECT_THROW(minta::detail::JsonValue::parse("{"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("[1,"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\""), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("{\"a\":"), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserInvalidNumberNoDigits) {
    EXPECT_THROW(minta::detail::JsonValue::parse("-"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("- "), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("-a"), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserInvalidDecimalNumber) {
    EXPECT_THROW(minta::detail::JsonValue::parse("1."), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("0.e5"), std::runtime_error);
    EXPECT_THROW(minta::detail::JsonValue::parse("1. "), std::runtime_error);
}

TEST_F(DynamicConfigTest, JsonParserNumberEdgeCases) {
    // 1e309 overflow: some platforms return inf (parsed as number),
    // others fail the stream (throw). Both are acceptable.
    try {
        auto inf_val = minta::detail::JsonValue::parse("1e309");
        EXPECT_TRUE(inf_val.isNumber());
    } catch (const std::runtime_error&) {
        // Platform stream failure — acceptable
    }

    try {
        auto neg_inf = minta::detail::JsonValue::parse("-1e309");
        EXPECT_TRUE(neg_inf.isNumber());
    } catch (const std::runtime_error&) {
        // Platform stream failure — acceptable
    }

    auto normal = minta::detail::JsonValue::parse("123.456e-7");
    EXPECT_TRUE(normal.isNumber());
}

TEST_F(DynamicConfigTest, JsonParserMaxCodepoint) {
    auto val = minta::detail::JsonValue::parse("\"\\uDBFF\\uDFFF\"");
    EXPECT_TRUE(val.isString());
}
