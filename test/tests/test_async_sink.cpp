#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include <lunar_log/sink/async_sink.hpp>
#include "utils/test_utils.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

// ---------------------------------------------------------------------------
// AsyncSink unit tests
// ---------------------------------------------------------------------------

class AsyncSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// Helper: in-memory sink that records entries
class RecordingSink : public minta::ISink {
public:
    RecordingSink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }

    void write(const minta::LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push_back(entry.message);
    }

    std::vector<std::string> messages() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages;
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.clear();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_messages;
};

// Helper: gate sink that blocks in write() until released (for drop-count testing)
class GateSink : public minta::ISink {
public:
    GateSink() : m_gate(true), m_count(0) {}

    void write(const minta::LogEntry&) override {
        m_count.fetch_add(1, std::memory_order_relaxed);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return !m_gate; });
    }

    void open() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_gate = false;
        }
        m_cv.notify_all();
    }

    size_t writeCount() const { return m_count.load(std::memory_order_relaxed); }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_gate;
    std::atomic<size_t> m_count;
};

// --- Test 1: Async write reaches inner sink ---
TEST_F(AsyncSinkTest, WriteReachesInnerSink) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::AsyncSink<minta::FileSink>>("async_test_write.txt");
    logger.info("Hello async world");
    logger.flush();

    TestUtils::waitForFileContent("async_test_write.txt");
    std::string content = TestUtils::readLogFile("async_test_write.txt");
    EXPECT_NE(content.find("Hello async world"), std::string::npos);
}

// --- Test 2: DropNewest — queue full, new items dropped ---
TEST_F(AsyncSinkTest, DropNewestPolicy) {
    minta::detail::BoundedQueue queue(2);

    minta::LogEntry e1;
    e1.level = minta::LogLevel::INFO;
    e1.message = "msg1";
    e1.timestamp = std::chrono::system_clock::now();
    EXPECT_TRUE(queue.push(std::move(e1), minta::OverflowPolicy::DropNewest));

    minta::LogEntry e2;
    e2.level = minta::LogLevel::INFO;
    e2.message = "msg2";
    e2.timestamp = std::chrono::system_clock::now();
    EXPECT_TRUE(queue.push(std::move(e2), minta::OverflowPolicy::DropNewest));

    // Queue is now full — next push should drop
    minta::LogEntry e3;
    e3.level = minta::LogLevel::INFO;
    e3.message = "msg3";
    e3.timestamp = std::chrono::system_clock::now();
    EXPECT_FALSE(queue.push(std::move(e3), minta::OverflowPolicy::DropNewest));

    EXPECT_EQ(queue.size(), 2u);
}

// --- Test 3: DropOldest — oldest items removed ---
TEST_F(AsyncSinkTest, DropOldestPolicy) {
    minta::detail::BoundedQueue queue(2);

    minta::LogEntry e1;
    e1.message = "oldest";
    e1.timestamp = std::chrono::system_clock::now();
    queue.push(std::move(e1), minta::OverflowPolicy::DropOldest);

    minta::LogEntry e2;
    e2.message = "middle";
    e2.timestamp = std::chrono::system_clock::now();
    queue.push(std::move(e2), minta::OverflowPolicy::DropOldest);

    // Full — this should drop "oldest"
    minta::LogEntry e3;
    e3.message = "newest";
    e3.timestamp = std::chrono::system_clock::now();
    EXPECT_TRUE(queue.push(std::move(e3), minta::OverflowPolicy::DropOldest));

    EXPECT_EQ(queue.size(), 2u);

    // Drain and verify "oldest" was dropped
    std::vector<minta::LogEntry> drained;
    queue.drain(drained);
    ASSERT_EQ(drained.size(), 2u);
    EXPECT_EQ(drained[0].message, "middle");
    EXPECT_EQ(drained[1].message, "newest");
}

// --- Test 4: Block — waits until space is available ---
TEST_F(AsyncSinkTest, BlockPolicy) {
    minta::detail::BoundedQueue queue(1);

    minta::LogEntry e1;
    e1.message = "first";
    e1.timestamp = std::chrono::system_clock::now();
    queue.push(std::move(e1), minta::OverflowPolicy::Block);

    std::atomic<bool> pushed(false);

    // Try to push in another thread (should block)
    std::thread producer([&] {
        minta::LogEntry e2;
        e2.message = "second";
        e2.timestamp = std::chrono::system_clock::now();
        queue.push(std::move(e2), minta::OverflowPolicy::Block);
        pushed.store(true, std::memory_order_release);
    });

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(pushed.load(std::memory_order_acquire));

    // Drain the queue — producer should unblock
    std::vector<minta::LogEntry> drained;
    queue.drain(drained);

    // Wait for producer to finish
    producer.join();
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
}

// --- Test 5: Graceful shutdown flushes remaining ---
TEST_F(AsyncSinkTest, GracefulShutdownFlushesRemaining) {
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::AsyncSink<minta::FileSink>>("async_test_shutdown.txt");

        for (int i = 0; i < 10; ++i) {
            logger.info("Shutdown test message {i}", i);
        }
        logger.flush();
    } // Logger and AsyncSink destroyed here — should flush remaining

    TestUtils::waitForFileContent("async_test_shutdown.txt");
    std::string content = TestUtils::readLogFile("async_test_shutdown.txt");
    EXPECT_NE(content.find("Shutdown test message"), std::string::npos);
}

// --- Test 6: Flush synchronization ---
TEST_F(AsyncSinkTest, FlushSynchronization) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    minta::AsyncOptions opts;
    opts.queueSize = 8192;
    logger.addSink<minta::AsyncSink<minta::FileSink>>(opts, std::string("async_test_flush.txt"));

    logger.info("Before flush");
    logger.flush();

    std::string content = TestUtils::readLogFile("async_test_flush.txt");
    EXPECT_NE(content.find("Before flush"), std::string::npos);
}

// --- Test 7: Multi-sink — sync + async mixed ---
TEST_F(AsyncSinkTest, MultiSinkSyncAndAsyncMixed) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("async_test_sync.txt");
    logger.addSink<minta::AsyncSink<minta::FileSink>>("async_test_async.txt");

    logger.info("Multi-sink test");
    logger.flush();

    // Both sinks should have the message
    std::string syncContent = TestUtils::readLogFile("async_test_sync.txt");
    EXPECT_NE(syncContent.find("Multi-sink test"), std::string::npos);

    std::string asyncContent = TestUtils::readLogFile("async_test_async.txt");
    EXPECT_NE(asyncContent.find("Multi-sink test"), std::string::npos);
}

// --- Test 8: FIFO order for single producer ---
TEST_F(AsyncSinkTest, FifoOrderSingleProducer) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::AsyncSink<minta::FileSink>>("async_test_order.txt");

    for (int i = 0; i < 20; ++i) {
        logger.info("Seq {idx}", i);
    }
    logger.flush();

    TestUtils::waitForFileContent("async_test_order.txt", 30);
    std::string content = TestUtils::readLogFile("async_test_order.txt");

    // Verify messages appear in FIFO order
    size_t lastPos = 0;
    for (int i = 0; i < 20; ++i) {
        std::string expected = "Seq " + std::to_string(i);
        size_t pos = content.find(expected, lastPos);
        EXPECT_NE(pos, std::string::npos) << "Missing: " << expected
            << "\nContent so far:\n" << content.substr(0, 500);
        if (pos != std::string::npos) {
            lastPos = pos + expected.size();
        }
    }
}

// --- Test 9: BoundedQueue drain ---
TEST_F(AsyncSinkTest, BoundedQueueDrain) {
    minta::detail::BoundedQueue queue(100);

    for (int i = 0; i < 5; ++i) {
        minta::LogEntry e;
        e.message = "entry_" + std::to_string(i);
        e.timestamp = std::chrono::system_clock::now();
        queue.push(std::move(e), minta::OverflowPolicy::DropNewest);
    }

    EXPECT_EQ(queue.size(), 5u);

    std::vector<minta::LogEntry> drained;
    size_t count = queue.drain(drained);
    EXPECT_EQ(count, 5u);
    EXPECT_EQ(drained.size(), 5u);
    EXPECT_TRUE(queue.empty());

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(drained[i].message, "entry_" + std::to_string(i));
    }
}

// --- Test 10: Fluent builder with AsyncSink ---
TEST_F(AsyncSinkTest, FluentBuilderWithAsyncSink) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::AsyncSink<minta::FileSink>>("async_test_fluent.txt")
        .build();

    logger.info("Fluent async test");
    logger.flush();

    TestUtils::waitForFileContent("async_test_fluent.txt");
    std::string content = TestUtils::readLogFile("async_test_fluent.txt");
    EXPECT_NE(content.find("Fluent async test"), std::string::npos);
}

// --- Test 11: DroppedCount increments on DropNewest overflow ---
TEST_F(AsyncSinkTest, DroppedCountIncrementsOnDropNewest) {
    minta::AsyncOptions opts;
    opts.queueSize = 1;
    opts.overflowPolicy = minta::OverflowPolicy::DropNewest;

    minta::AsyncSink<GateSink> sink(opts);

    // First write: consumer picks it up and blocks in GateSink
    {
        minta::LogEntry e;
        e.level = minta::LogLevel::INFO;
        e.message = "first";
        e.timestamp = std::chrono::system_clock::now();
        sink.write(e);
    }

    // Wait for consumer to dequeue the entry and block in GateSink::write()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Second write fills the queue (capacity=1)
    {
        minta::LogEntry e;
        e.level = minta::LogLevel::INFO;
        e.message = "fills_queue";
        e.timestamp = std::chrono::system_clock::now();
        sink.write(e);
    }

    // These 5 writes should all be dropped (queue full, DropNewest)
    const size_t numToDrop = 5;
    for (size_t i = 0; i < numToDrop; ++i) {
        minta::LogEntry e;
        e.level = minta::LogLevel::INFO;
        e.message = "dropped";
        e.timestamp = std::chrono::system_clock::now();
        sink.write(e);
    }

    EXPECT_EQ(sink.droppedCount(), numToDrop);

    // Release gate for clean shutdown
    sink.innerSink()->open();
}

// --- Test 12: Periodic flush via flushIntervalMs ---
TEST_F(AsyncSinkTest, PeriodicFlushViaFlushInterval) {
    minta::AsyncOptions opts;
    opts.queueSize = 8192;
    opts.flushIntervalMs = 100;

    minta::AsyncSink<RecordingSink> sink(opts);

    {
        minta::LogEntry e;
        e.level = minta::LogLevel::INFO;
        e.message = "periodic test";
        e.timestamp = std::chrono::system_clock::now();
        sink.write(e);
    }

    // Do NOT call flush() — rely on consumer thread to deliver
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GE(sink.innerSink()->count(), 1u);
    std::vector<std::string> msgs = sink.innerSink()->messages();
    ASSERT_FALSE(msgs.empty());
    EXPECT_EQ(msgs[0], "periodic test");
}

// --- Test 13: Concurrent flush does not spin (regression for flushPending leak) ---
TEST_F(AsyncSinkTest, ConcurrentFlushNoSpin) {
    minta::AsyncOptions opts;
    opts.queueSize = 8192;
    opts.overflowPolicy = minta::OverflowPolicy::DropNewest;

    minta::AsyncSink<RecordingSink> sink(opts);

    const int kThreads = 4;
    const int kWritesPerThread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&sink, t] {
            for (int i = 0; i < kWritesPerThread; ++i) {
                minta::LogEntry e;
                e.level = minta::LogLevel::INFO;
                e.message = "t" + std::to_string(t) + "_" + std::to_string(i);
                e.timestamp = std::chrono::system_clock::now();
                sink.write(e);
            }
            sink.flush();
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    size_t total = sink.innerSink()->count() + sink.droppedCount();
    EXPECT_EQ(total, static_cast<size_t>(kThreads * kWritesPerThread));
    EXPECT_EQ(sink.droppedCount(), 0u);
}
