#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/global.hpp"
#include "lunar_log/sink/callback_sink.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <mutex>
#include <thread>

class GlobalLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test
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

// --- Throws before configure ---

TEST_F(GlobalLoggerTest, ThrowsBeforeConfigure) {
    EXPECT_FALSE(minta::Log::isInitialized());
    EXPECT_THROW(minta::Log::info("should throw"), std::logic_error);
    EXPECT_THROW(minta::Log::warn("should throw"), std::logic_error);
    EXPECT_THROW(minta::Log::error("should throw"), std::logic_error);
    EXPECT_THROW(minta::Log::instance(), std::logic_error);
}

// --- Basic logging after init ---

struct GCaptured {
    minta::LogLevel level;
    std::string message;
};

TEST_F(GlobalLoggerTest, BasicInfoWarnErrorAfterInit) {
    std::vector<GCaptured> captured;
    std::mutex mtx;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            std::lock_guard<std::mutex> lock(mtx);
            captured.push_back({entry.level, entry.message});
        }));
    logger.addCustomSink(std::move(sink));

    minta::Log::init(std::move(logger));
    EXPECT_TRUE(minta::Log::isInitialized());

    minta::Log::info("Info message {v}", 1);
    minta::Log::warn("Warn message {v}", 2);
    minta::Log::error("Error message {v}", 3);
    minta::Log::instance().flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::lock_guard<std::mutex> lock(mtx);
    ASSERT_GE(captured.size(), 3u);
    EXPECT_EQ(captured[0].level, minta::LogLevel::INFO);
    EXPECT_EQ(captured[1].level, minta::LogLevel::WARN);
    EXPECT_EQ(captured[2].level, minta::LogLevel::ERROR);
    EXPECT_TRUE(captured[0].message.find("Info message 1") != std::string::npos);
    EXPECT_TRUE(captured[1].message.find("Warn message 2") != std::string::npos);
    EXPECT_TRUE(captured[2].message.find("Error message 3") != std::string::npos);
}

// --- Shutdown then throws again ---

TEST_F(GlobalLoggerTest, ShutdownThenThrows) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::ConsoleSink>()
        .build();

    minta::Log::init(std::move(logger));
    EXPECT_TRUE(minta::Log::isInitialized());

    // Should work
    EXPECT_NO_THROW(minta::Log::info("before shutdown"));
    minta::Log::instance().flush();

    minta::Log::shutdown();
    EXPECT_FALSE(minta::Log::isInitialized());

    // Should throw after shutdown
    EXPECT_THROW(minta::Log::info("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::warn("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::trace("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::debug("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::error("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::fatal("after shutdown"), std::logic_error);
    EXPECT_THROW(minta::Log::instance(), std::logic_error);
}

// --- Double init replaces instance ---

TEST_F(GlobalLoggerTest, DoubleInitReplacesInstance) {
    std::vector<std::string> captured1;
    std::vector<std::string> captured2;
    std::mutex mtx1, mtx2;

    auto logger1 = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink1 = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            std::lock_guard<std::mutex> lock(mtx1);
            captured1.push_back(entry.message);
        }));
    logger1.addCustomSink(std::move(sink1));
    minta::Log::init(std::move(logger1));

    minta::Log::info("To first logger");
    minta::Log::instance().flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto logger2 = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink2 = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            std::lock_guard<std::mutex> lock(mtx2);
            captured2.push_back(entry.message);
        }));
    logger2.addCustomSink(std::move(sink2));
    minta::Log::init(std::move(logger2));

    minta::Log::info("To second logger");
    minta::Log::instance().flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        std::lock_guard<std::mutex> lock(mtx1);
        EXPECT_GE(captured1.size(), 1u);
        EXPECT_TRUE(captured1[0].find("To first logger") != std::string::npos);
    }
    {
        std::lock_guard<std::mutex> lock(mtx2);
        EXPECT_GE(captured2.size(), 1u);
        EXPECT_TRUE(captured2[0].find("To second logger") != std::string::npos);
    }

    {
        std::lock_guard<std::mutex> lock(mtx1);
        for (const auto& msg : captured1) {
            EXPECT_TRUE(msg.find("To second logger") == std::string::npos);
        }
    }
}

// --- isInitialized lifecycle ---

TEST_F(GlobalLoggerTest, IsInitializedLifecycle) {
    // Start: not initialized
    EXPECT_FALSE(minta::Log::isInitialized());

    // After init: initialized
    auto logger = minta::LunarLog::configure()
        .writeTo<minta::ConsoleSink>()
        .build();
    minta::Log::init(std::move(logger));
    EXPECT_TRUE(minta::Log::isInitialized());

    // After shutdown: not initialized
    minta::Log::shutdown();
    EXPECT_FALSE(minta::Log::isInitialized());

    // After re-init: initialized again
    auto logger2 = minta::LunarLog::configure()
        .writeTo<minta::ConsoleSink>()
        .build();
    minta::Log::init(std::move(logger2));
    EXPECT_TRUE(minta::Log::isInitialized());
}

// --- All log levels ---

TEST_F(GlobalLoggerTest, AllLogLevelsWork) {
    std::vector<GCaptured> entries;
    std::mutex mtx;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            std::lock_guard<std::mutex> lock(mtx);
            entries.push_back({entry.level, entry.message});
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    minta::Log::trace("t");
    minta::Log::debug("d");
    minta::Log::info("i");
    minta::Log::warn("w");
    minta::Log::error("e");
    minta::Log::fatal("f");
    minta::Log::instance().flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::lock_guard<std::mutex> lock(mtx);
    ASSERT_GE(entries.size(), 6u);
    EXPECT_EQ(entries[0].level, minta::LogLevel::TRACE);
    EXPECT_EQ(entries[1].level, minta::LogLevel::DEBUG);
    EXPECT_EQ(entries[2].level, minta::LogLevel::INFO);
    EXPECT_EQ(entries[3].level, minta::LogLevel::WARN);
    EXPECT_EQ(entries[4].level, minta::LogLevel::ERROR);
    EXPECT_EQ(entries[5].level, minta::LogLevel::FATAL);
}

// --- Double shutdown is safe ---

TEST_F(GlobalLoggerTest, DoubleShutdownIsSafe) {
    auto logger = minta::LunarLog::configure()
        .writeTo<minta::ConsoleSink>()
        .build();
    minta::Log::init(std::move(logger));

    EXPECT_NO_THROW(minta::Log::shutdown());
    EXPECT_NO_THROW(minta::Log::shutdown()); // second shutdown is no-op
    EXPECT_FALSE(minta::Log::isInitialized());
}

// --- Instance access ---

TEST_F(GlobalLoggerTest, InstanceReturnsConfiguredLogger) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::WARN)
        .writeTo<minta::ConsoleSink>()
        .build();
    minta::Log::init(std::move(logger));

    EXPECT_NO_THROW({
        auto& inst = minta::Log::instance();
        EXPECT_EQ(inst.getMinLevel(), minta::LogLevel::WARN);
    });
}
