#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <vector>
#include <atomic>

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(ThreadSafetyTest, ConcurrentLoggingFromMultipleThreads) {
    static const int kThreadCount = 8;
    static const int kMessagesPerThread = 100;

    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("thread_safety_test.txt");

    std::vector<std::thread> threads;
    std::atomic<int> startFlag(0);

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&logger, &startFlag, t]() {
            while (startFlag.load(std::memory_order_acquire) == 0) {
                std::this_thread::yield();
            }
            for (int i = 0; i < kMessagesPerThread; ++i) {
                logger.info("Thread {id} message {num}", t, i);
            }
        });
    }

    startFlag.store(1, std::memory_order_release);

    for (auto &th : threads) {
        th.join();
    }

    logger.flush();

    TestUtils::waitForFileContent("thread_safety_test.txt");
    std::string logContent = TestUtils::readLogFile("thread_safety_test.txt");

    int lineCount = 0;
    for (size_t pos = 0; pos < logContent.size(); ++pos) {
        if (logContent[pos] == '\n') ++lineCount;
    }

    EXPECT_GE(lineCount, kThreadCount * kMessagesPerThread);

    for (int t = 0; t < kThreadCount; ++t) {
        std::string marker = "Thread " + std::to_string(t) + " message ";
        EXPECT_TRUE(logContent.find(marker) != std::string::npos)
            << "Missing messages from thread " << t;
    }
}

TEST_F(ThreadSafetyTest, ConcurrentSetContextClearContextWhileLogging) {
    static const int kThreadCount = 4;
    static const int kIterations = 200;

    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("thread_safety_test.txt");
    logger.setCaptureSourceLocation(true);

    std::atomic<int> startFlag(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&logger, &startFlag, t]() {
            while (startFlag.load(std::memory_order_acquire) == 0) {
                std::this_thread::yield();
            }
            for (int i = 0; i < kIterations; ++i) {
                std::string key = "key_" + std::to_string(t);
                std::string val = "val_" + std::to_string(i);
                logger.setContext(key, val);
                logger.info("ctx msg {t} {i}", t, i);
                logger.clearContext(key);
            }
        });
    }

    startFlag.store(1, std::memory_order_release);

    for (auto &th : threads) {
        th.join();
    }

    logger.flush();

    TestUtils::waitForFileContent("thread_safety_test.txt");
    std::string logContent = TestUtils::readLogFile("thread_safety_test.txt");

    for (int t = 0; t < kThreadCount; ++t) {
        std::string marker = "ctx msg " + std::to_string(t);
        EXPECT_TRUE(logContent.find(marker) != std::string::npos)
            << "Missing context messages from thread " << t;
    }
}
