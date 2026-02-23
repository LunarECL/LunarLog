#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/macros.hpp"
#include "lunar_log/global.hpp"
#include "lunar_log/sink/callback_sink.hpp"
#include "utils/test_utils.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>

class KeyValueArgsTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (minta::Log::isInitialized()) {
            minta::Log::shutdown();
        }
        TestUtils::cleanupLogFiles();
    }
    void TearDown() override {
        if (minta::Log::isInitialized()) {
            minta::Log::shutdown();
        }
        TestUtils::cleanupLogFiles();
    }
};

struct KVCaptured {
    minta::LogLevel level;
    std::string message;
    std::string templateStr;
    std::vector<std::pair<std::string, std::string>> arguments;
    std::vector<minta::PlaceholderProperty> properties;
};

static KVCaptured kvCapture(const minta::LogEntry& entry) {
    return {entry.level, entry.message, entry.templateStr, entry.arguments, entry.properties};
}

// ---------------------------------------------------------------------------
// 1. Key-value basic
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, BasicKeyValue) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{name} from {ip}", "name", "alice", "ip", "10.0.0.1");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "alice from 10.0.0.1");
    ASSERT_EQ(captured[0].arguments.size(), 2u);
    EXPECT_EQ(captured[0].arguments[0].first, "name");
    EXPECT_EQ(captured[0].arguments[0].second, "alice");
    EXPECT_EQ(captured[0].arguments[1].first, "ip");
    EXPECT_EQ(captured[0].arguments[1].second, "10.0.0.1");
}

// ---------------------------------------------------------------------------
// 2. Positional backward compatibility
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, PositionalBackwardCompat) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{name} from {ip}", "alice", "10.0.0.1");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "alice from 10.0.0.1");
    ASSERT_EQ(captured[0].arguments.size(), 2u);
    EXPECT_EQ(captured[0].arguments[0].first, "name");
    EXPECT_EQ(captured[0].arguments[0].second, "alice");
    EXPECT_EQ(captured[0].arguments[1].first, "ip");
    EXPECT_EQ(captured[0].arguments[1].second, "10.0.0.1");
}

// ---------------------------------------------------------------------------
// 3. Key-value with format specifiers
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueWithFormatSpec) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("Price: {amount:.2f}", "amount", 3.14159);
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "Price: 3.14");
}

// ---------------------------------------------------------------------------
// 4. Key-value with pipe transforms
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueWithPipeTransform) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{name|upper}", "name", "alice");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "ALICE");
}

// ---------------------------------------------------------------------------
// 5. Key-value out of order
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueOutOfOrder) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{b} then {a}", "a", "first", "b", "second");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "second then first");
}

// ---------------------------------------------------------------------------
// 6. Key-value with indexed placeholder (mixed mode — falls back to positional)
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueWithIndexedFallsBackToPositional) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{0} and {name}", "zero", "name", "alice");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_FALSE(captured[0].message.empty());
}

// ---------------------------------------------------------------------------
// 7. Key-value unknown key (no key matches → falls back to positional)
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, UnknownKeyFallsBackToPositional) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{name}", "wrong", "alice");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "wrong");
}

// ---------------------------------------------------------------------------
// 8. JSON structured output with key-value args
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, JsonStructuredOutputWithKeyValue) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("kv_json_test.txt");

    logger.info("User {name} from {ip}", "name", "alice", "ip", "10.0.0.1");

    logger.flush();
    TestUtils::waitForFileContent("kv_json_test.txt");
    std::string content = TestUtils::readLogFile("kv_json_test.txt");

    EXPECT_TRUE(content.find("\"message\":\"User alice from 10.0.0.1\"") != std::string::npos)
        << "Got: " << content;
    EXPECT_TRUE(content.find("\"name\":\"alice\"") != std::string::npos)
        << "Got: " << content;
    EXPECT_TRUE(content.find("\"ip\":\"10.0.0.1\"") != std::string::npos)
        << "Got: " << content;
}

// ---------------------------------------------------------------------------
// 9. XML structured output with key-value args
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, XmlStructuredOutputWithKeyValue) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("kv_xml_test.txt");

    logger.info("User {name} from {ip}", "name", "alice", "ip", "10.0.0.1");

    logger.flush();
    TestUtils::waitForFileContent("kv_xml_test.txt");
    std::string content = TestUtils::readLogFile("kv_xml_test.txt");

    EXPECT_TRUE(content.find("<message>User alice from 10.0.0.1</message>") != std::string::npos)
        << "Got: " << content;
    EXPECT_TRUE(content.find("<name>alice</name>") != std::string::npos)
        << "Got: " << content;
    EXPECT_TRUE(content.find("<ip>10.0.0.1</ip>") != std::string::npos)
        << "Got: " << content;
}

// ---------------------------------------------------------------------------
// 10. Source location macros with key-value
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, SourceLocationMacroWithKeyValue) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .captureSourceLocation(true)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    LUNAR_INFO(logger, "{name}", "name", "alice");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "alice");
}

// ---------------------------------------------------------------------------
// 11. Global logger with key-value
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, GlobalLoggerWithKeyValue) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    minta::Log::init(std::move(logger));
    minta::Log::info("{name}", "name", "alice");
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "alice");
}

// ---------------------------------------------------------------------------
// 12. Key-value properties propagated correctly
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValuePropertiesCorrect) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("{name} from {ip}", "ip", "10.0.0.1", "name", "alice");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "alice from 10.0.0.1");
    ASSERT_EQ(captured[0].properties.size(), 2u);
    EXPECT_EQ(captured[0].properties[0].name, "name");
    EXPECT_EQ(captured[0].properties[0].value, "alice");
    EXPECT_EQ(captured[0].properties[1].name, "ip");
    EXPECT_EQ(captured[0].properties[1].value, "10.0.0.1");
}

// ---------------------------------------------------------------------------
// 13. Key-value with file output (human readable)
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueFileOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("kv_file_test.txt");

    logger.info("User {name} logged in from {ip}", "name", "alice", "ip", "192.168.1.1");

    logger.flush();
    TestUtils::waitForFileContent("kv_file_test.txt");
    std::string content = TestUtils::readLogFile("kv_file_test.txt");

    EXPECT_TRUE(content.find("User alice logged in from 192.168.1.1") != std::string::npos)
        << "Got: " << content;
}

// ---------------------------------------------------------------------------
// 14. Key-value single placeholder
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, SinglePlaceholderKeyValue) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("Hello {name}!", "name", "world");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    EXPECT_EQ(captured[0].message, "Hello world!");
}

// ---------------------------------------------------------------------------
// 15. Key-value with multiple log levels
// ---------------------------------------------------------------------------

TEST_F(KeyValueArgsTest, KeyValueMultipleLevels) {
    std::vector<KVCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(kvCapture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.trace("{k}", "k", "t");
    logger.debug("{k}", "k", "d");
    logger.info("{k}", "k", "i");
    logger.warn("{k}", "k", "w");
    logger.error("{k}", "k", "e");
    logger.fatal("{k}", "k", "f");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 6u; }));

    EXPECT_EQ(captured[0].message, "t");
    EXPECT_EQ(captured[1].message, "d");
    EXPECT_EQ(captured[2].message, "i");
    EXPECT_EQ(captured[3].message, "w");
    EXPECT_EQ(captured[4].message, "e");
    EXPECT_EQ(captured[5].message, "f");
}
