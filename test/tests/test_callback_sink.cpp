#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/sink/callback_sink.hpp"
#include "utils/test_utils.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>

class CallbackSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- EntryCallback tests ---

struct CapturedEntry {
    minta::LogLevel level;
    std::string message;
    std::string templateStr;
    std::vector<std::pair<std::string, std::string>> arguments;
};

static CapturedEntry capture(const minta::LogEntry& entry) {
    return {entry.level, entry.message, entry.templateStr, entry.arguments};
}

TEST_F(CallbackSinkTest, EntryCallbackReceivesCorrectFields) {
    std::vector<CapturedEntry> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(capture(entry));
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("Hello {name}", "world");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));

    const auto& e = captured[0];
    EXPECT_EQ(e.level, minta::LogLevel::INFO);
    EXPECT_TRUE(e.message.find("Hello world") != std::string::npos);
    EXPECT_EQ(e.templateStr, "Hello {name}");
}

TEST_F(CallbackSinkTest, EntryCallbackReceivesCorrectLevel) {
    std::vector<minta::LogLevel> levels;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                levels.push_back(entry.level);
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.trace("trace msg");
    logger.debug("debug msg");
    logger.warn("warn msg");
    logger.error("error msg");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return levels.size() >= 4u; }));
    EXPECT_EQ(levels[0], minta::LogLevel::TRACE);
    EXPECT_EQ(levels[1], minta::LogLevel::DEBUG);
    EXPECT_EQ(levels[2], minta::LogLevel::WARN);
    EXPECT_EQ(levels[3], minta::LogLevel::ERROR);
}

TEST_F(CallbackSinkTest, StringCallbackReceivesFormattedString) {
    std::vector<std::string> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::StringCallback([&](const std::string& msg) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(msg);
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("Formatted {val}", "test");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));
    EXPECT_TRUE(captured[0].find("@t") != std::string::npos);
    EXPECT_TRUE(captured[0].find("Formatted {val}") != std::string::npos);
}

TEST_F(CallbackSinkTest, StringCallbackWithCustomFormatter) {
    std::vector<std::string> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::StringCallback([&](const std::string& msg) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(msg);
            }
            cv.notify_all();
        }),
        minta::detail::make_unique<minta::HumanReadableFormatter>());
    logger.addCustomSink(std::move(sink));

    logger.info("Human readable {val}", "test");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));
    EXPECT_TRUE(captured[0].find("[INFO]") != std::string::npos);
    EXPECT_TRUE(captured[0].find("Human readable test") != std::string::npos);
}

TEST_F(CallbackSinkTest, MultipleWritesInvokeCallbackEachTime) {
    int callCount = 0;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry&) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                ++callCount;
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("msg 1");
    logger.info("msg 2");
    logger.info("msg 3");
    logger.warn("msg 4");
    logger.error("msg 5");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return callCount >= 5; }));
    EXPECT_EQ(callCount, 5);
}

TEST_F(CallbackSinkTest, NullEntryCallbackDoesNotCrash) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback(nullptr));
    logger.addCustomSink(std::move(sink));

    logger.info("Should not crash 1");
    logger.warn("Should not crash 2");
    logger.error("Should not crash 3");
    logger.flush();
}

TEST_F(CallbackSinkTest, NullStringCallbackDoesNotCrash) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::StringCallback(nullptr));
    logger.addCustomSink(std::move(sink));

    logger.info("Should not crash 1");
    logger.warn("Should not crash 2");
    logger.error("Should not crash 3");
    logger.flush();
}

TEST_F(CallbackSinkTest, FluentBuilderEntryCallback) {
    std::vector<CapturedEntry> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto entryCb = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back(capture(entry));
            }
            cv.notify_all();
        }));

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .build();
    logger.addCustomSink(std::move(entryCb));

    logger.debug("Builder callback {x}", 42);
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));
    EXPECT_EQ(captured[0].level, minta::LogLevel::DEBUG);
    EXPECT_TRUE(captured[0].message.find("Builder callback 42") != std::string::npos);
}

TEST_F(CallbackSinkTest, EntryCallbackSeesArguments) {
    std::vector<std::pair<std::string, std::string>> capturedArgs;
    std::mutex mtx;
    std::condition_variable cv;
    int callCount = 0;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                capturedArgs = entry.arguments;
                ++callCount;
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("User {name} age {age}", "alice", 30);
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return callCount >= 1; }));
    ASSERT_EQ(capturedArgs.size(), 2u);
    EXPECT_EQ(capturedArgs[0].first, "name");
    EXPECT_EQ(capturedArgs[0].second, "alice");
    EXPECT_EQ(capturedArgs[1].first, "age");
    EXPECT_EQ(capturedArgs[1].second, "30");
}

TEST_F(CallbackSinkTest, ThrowingEntryCallbackDoesNotCrash) {
    std::atomic<int> delivered(0);
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry&) {
            int n = delivered.fetch_add(1, std::memory_order_relaxed);
            if (n == 0) {
                throw std::runtime_error("boom");
            }
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("first - triggers throw");
    logger.info("second - should still arrive");
    logger.info("third - should still arrive");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return delivered.load(std::memory_order_relaxed) >= 3; });

    EXPECT_GE(delivered.load(std::memory_order_relaxed), 2);
}

TEST_F(CallbackSinkTest, ThrowingStringCallbackDoesNotCrash) {
    std::atomic<int> delivered(0);
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::StringCallback([&](const std::string&) {
            int n = delivered.fetch_add(1, std::memory_order_relaxed);
            if (n == 0) {
                throw std::runtime_error("boom");
            }
        }));
    logger.addCustomSink(std::move(sink));

    logger.info("first - triggers throw");
    logger.info("second - should still arrive");
    logger.info("third - should still arrive");
    logger.flush();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return delivered.load(std::memory_order_relaxed) >= 3; });

    EXPECT_GE(delivered.load(std::memory_order_relaxed), 2);
}
