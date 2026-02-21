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
#include <condition_variable>
#include <atomic>

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
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back({entry.level, entry.message});
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));

    minta::Log::init(std::move(logger));
    EXPECT_TRUE(minta::Log::isInitialized());

    minta::Log::info("Info message {v}", 1);
    minta::Log::warn("Warn message {v}", 2);
    minta::Log::error("Error message {v}", 3);
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 3u; }));
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
    minta::Log::flush();

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
    std::condition_variable cv1, cv2;

    auto logger1 = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink1 = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx1);
                captured1.push_back(entry.message);
            }
            cv1.notify_all();
        }));
    logger1.addCustomSink(std::move(sink1));
    minta::Log::init(std::move(logger1));

    minta::Log::info("To first logger");
    minta::Log::flush();

    {
        std::unique_lock<std::mutex> lock(mtx1);
        ASSERT_TRUE(cv1.wait_for(lock, std::chrono::seconds(5),
            [&]() { return captured1.size() >= 1u; }));
    }

    auto logger2 = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink2 = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx2);
                captured2.push_back(entry.message);
            }
            cv2.notify_all();
        }));
    logger2.addCustomSink(std::move(sink2));
    minta::Log::init(std::move(logger2));

    minta::Log::info("To second logger");
    minta::Log::flush();

    {
        std::unique_lock<std::mutex> lock(mtx2);
        ASSERT_TRUE(cv2.wait_for(lock, std::chrono::seconds(5),
            [&]() { return captured2.size() >= 1u; }));
        EXPECT_TRUE(captured2[0].find("To second logger") != std::string::npos);
    }

    {
        std::lock_guard<std::mutex> lock(mtx1);
        EXPECT_TRUE(captured1[0].find("To first logger") != std::string::npos);
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
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                entries.push_back({entry.level, entry.message});
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    minta::Log::trace("t");
    minta::Log::debug("d");
    minta::Log::info("i");
    minta::Log::warn("w");
    minta::Log::error("e");
    minta::Log::fatal("f");
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return entries.size() >= 6u; }));
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
        auto inst = minta::Log::instance();
        EXPECT_EQ(inst->getMinLevel(), minta::LogLevel::WARN);
    });
}

// --- Log::configure().build() sets the global instance ---

TEST_F(GlobalLoggerTest, ConfigureBuildSetsGlobalInstance) {
    EXPECT_FALSE(minta::Log::isInitialized());

    minta::Log::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .writeTo<minta::ConsoleSink>()
        .build();

    EXPECT_TRUE(minta::Log::isInitialized());
    EXPECT_NO_THROW(minta::Log::info("configured via build"));
    minta::Log::flush();
}

TEST_F(GlobalLoggerTest, ConfigureBuildWithCallbackSink) {
    std::vector<GCaptured> captured;
    std::mutex mtx;
    std::condition_variable cv;

    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                captured.push_back({entry.level, entry.message});
            }
            cv.notify_all();
        }));

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    minta::Log::info("Via configure build {v}", 42);
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return captured.size() >= 1u; }));
    EXPECT_EQ(captured[0].level, minta::LogLevel::INFO);
    EXPECT_TRUE(captured[0].message.find("Via configure build 42") != std::string::npos);
}

TEST_F(GlobalLoggerTest, ConfigureBuildWithMinLevel) {
    minta::Log::configure()
        .minLevel(minta::LogLevel::WARN)
        .writeTo<minta::ConsoleSink>()
        .build();

    auto inst = minta::Log::instance();
    EXPECT_EQ(inst->getMinLevel(), minta::LogLevel::WARN);
}

// --- Log::flush() convenience ---

TEST_F(GlobalLoggerTest, FlushThrowsBeforeInit) {
    EXPECT_THROW(minta::Log::flush(), std::logic_error);
}

TEST_F(GlobalLoggerTest, FlushWorksAfterInit) {
    minta::Log::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::ConsoleSink>()
        .build();

    EXPECT_NO_THROW({
        minta::Log::info("flush test");
        minta::Log::flush();
    });
}

// --- Instance returns shared_ptr ---

TEST_F(GlobalLoggerTest, InstanceReturnsSharedPtr) {
    minta::Log::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::ConsoleSink>()
        .build();

    auto inst = minta::Log::instance();
    EXPECT_NE(inst, nullptr);
    EXPECT_NO_THROW(inst->info("via shared_ptr"));
    inst->flush();
}

// --- Concurrent stress test ---

TEST_F(GlobalLoggerTest, ConcurrentLoggingWithShutdownAndReinit) {
    const int kNumLogThreads = 8;
    const int kMessagesPerThread = 200;

    std::atomic<int> delivered(0);
    std::mutex mtx;
    std::condition_variable cv;

    auto makeLogger = [&]() {
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::TRACE)
            .build();
        auto sink = minta::detail::make_unique<minta::CallbackSink>(
            minta::CallbackSink::EntryCallback([&](const minta::LogEntry&) {
                delivered.fetch_add(1, std::memory_order_relaxed);
                cv.notify_all();
            }));
        logger.addCustomSink(std::move(sink));
        return logger;
    };

    minta::Log::init(makeLogger());

    std::atomic<bool> stop(false);
    std::vector<std::thread> logThreads;

    for (int i = 0; i < kNumLogThreads; ++i) {
        logThreads.emplace_back([&, i]() {
            for (int j = 0; j < kMessagesPerThread; ++j) {
                try {
                    minta::Log::info("thread {t} msg {m}", i, j);
                } catch (const std::logic_error&) {
                    // Expected during shutdown window
                }
            }
        });
    }

    std::thread churnThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        minta::Log::shutdown();
        minta::Log::init(makeLogger());
    });

    for (auto& t : logThreads) t.join();
    churnThread.join();

    if (minta::Log::isInitialized()) {
        minta::Log::flush();
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(5),
            [&]() { return delivered.load(std::memory_order_relaxed) > 0; });
    }

    EXPECT_GT(delivered.load(std::memory_order_relaxed), 0);
}

// --- Dynamic-level log() ---

TEST_F(GlobalLoggerTest, LogWithDynamicLevel) {
    std::vector<GCaptured> entries;
    std::mutex mtx;
    std::condition_variable cv;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                entries.push_back({entry.level, entry.message});
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    minta::LogLevel dynamicLevel = minta::LogLevel::WARN;
    minta::Log::log(dynamicLevel, "Dynamic {v}", 99);
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return entries.size() >= 1u; }));
    EXPECT_EQ(entries[0].level, minta::LogLevel::WARN);
    EXPECT_TRUE(entries[0].message.find("Dynamic 99") != std::string::npos);
}

// --- Exception-logging overloads ---

TEST_F(GlobalLoggerTest, ErrorWithExceptionAttachment) {
    std::vector<GCaptured> entries;
    std::mutex mtx;
    std::condition_variable cv;
    bool hasException = false;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                entries.push_back({entry.level, entry.message});
                hasException = entry.hasException();
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    std::runtime_error ex("test error");
    minta::Log::error(ex, "Something failed: {reason}", "disk full");
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return entries.size() >= 1u; }));
    EXPECT_EQ(entries[0].level, minta::LogLevel::ERROR);
    EXPECT_TRUE(entries[0].message.find("Something failed: disk full") != std::string::npos);
    EXPECT_TRUE(hasException);
}

TEST_F(GlobalLoggerTest, ExceptionOnlyOverload) {
    std::vector<GCaptured> entries;
    std::mutex mtx;
    std::condition_variable cv;
    bool hasException = false;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                entries.push_back({entry.level, entry.message});
                hasException = entry.hasException();
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    std::runtime_error ex("kaboom");
    minta::Log::fatal(ex);
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return entries.size() >= 1u; }));
    EXPECT_EQ(entries[0].level, minta::LogLevel::FATAL);
    EXPECT_TRUE(entries[0].message.find("kaboom") != std::string::npos);
    EXPECT_TRUE(hasException);
}

TEST_F(GlobalLoggerTest, LogDynamicLevelWithException) {
    std::vector<GCaptured> entries;
    std::mutex mtx;
    std::condition_variable cv;
    bool hasException = false;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    auto sink = minta::detail::make_unique<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback([&](const minta::LogEntry& entry) {
            {
                std::lock_guard<std::mutex> lock(mtx);
                entries.push_back({entry.level, entry.message});
                hasException = entry.hasException();
            }
            cv.notify_all();
        }));
    logger.addCustomSink(std::move(sink));
    minta::Log::init(std::move(logger));

    std::runtime_error ex("dynamic ex");
    minta::Log::log(minta::LogLevel::DEBUG, ex, "Failed at {step}", "init");
    minta::Log::flush();

    std::unique_lock<std::mutex> lock(mtx);
    ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
        [&]() { return entries.size() >= 1u; }));
    EXPECT_EQ(entries[0].level, minta::LogLevel::DEBUG);
    EXPECT_TRUE(entries[0].message.find("Failed at init") != std::string::npos);
    EXPECT_TRUE(hasException);
}

TEST_F(GlobalLoggerTest, ExceptionOverloadsThrowBeforeInit) {
    std::runtime_error ex("no logger");
    EXPECT_THROW(minta::Log::error(ex, "msg"), std::logic_error);
    EXPECT_THROW(minta::Log::error(ex), std::logic_error);
    EXPECT_THROW(minta::Log::log(minta::LogLevel::ERROR, ex), std::logic_error);
    EXPECT_THROW(minta::Log::log(minta::LogLevel::ERROR, "msg"), std::logic_error);
}
