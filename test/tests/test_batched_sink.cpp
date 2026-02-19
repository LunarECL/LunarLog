#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include <lunar_log/sink/batched_sink.hpp>
#include "utils/test_utils.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <stdexcept>

// ---------------------------------------------------------------------------
// BatchedSink unit tests
// ---------------------------------------------------------------------------

class BatchedSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// Mock batched sink for testing
class MockBatchedSink : public minta::BatchedSink {
public:
    explicit MockBatchedSink(minta::BatchOptions opts = minta::BatchOptions())
        : BatchedSink(opts), m_throwCount(0), m_flushCallCount(0) {}

    ~MockBatchedSink() noexcept {
        stopAndFlush();
    }

    std::vector<std::vector<std::string>> batches() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_batches;
    }

    size_t batchCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_batches.size();
    }

    size_t totalEntries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t total = 0;
        for (size_t i = 0; i < m_batches.size(); ++i) {
            total += m_batches[i].size();
        }
        return total;
    }

    size_t flushCallCount() const {
        return m_flushCallCount.load(std::memory_order_acquire);
    }

    std::vector<std::pair<std::string, size_t>> errors() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_errors;
    }

    void setThrowCount(int n) { m_throwCount.store(n, std::memory_order_release); }

protected:
    void writeBatch(const std::vector<const minta::LogEntry*>& batch) override {
        if (m_throwCount.load(std::memory_order_acquire) > 0) {
            m_throwCount.fetch_sub(1, std::memory_order_acq_rel);
            throw std::runtime_error("Mock batch error");
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> messages;
        for (size_t i = 0; i < batch.size(); ++i) {
            messages.push_back(batch[i]->message);
        }
        m_batches.push_back(std::move(messages));
    }

    void onFlush() override {
        m_flushCallCount.fetch_add(1, std::memory_order_release);
    }

    void onBatchError(const std::exception& e, size_t retryCount) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errors.push_back({std::string(e.what()), retryCount});
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::vector<std::string>> m_batches;
    std::atomic<int> m_throwCount;
    std::atomic<size_t> m_flushCallCount;
    std::vector<std::pair<std::string, size_t>> m_errors;
};

// Helper to create a LogEntry for testing
static minta::LogEntry makeEntry(const std::string& msg) {
    minta::LogEntry e;
    e.level = minta::LogLevel::INFO;
    e.message = msg;
    e.timestamp = std::chrono::system_clock::now();
    return e;
}

// --- Test 1: batchSize triggers writeBatch ---
TEST_F(BatchedSinkTest, BatchSizeTriggerFlush) {
    minta::BatchOptions opts;
    opts.setBatchSize(3).setFlushIntervalMs(0); // no timer
    auto sink = minta::detail::make_unique<MockBatchedSink>(opts);
    auto* ptr = sink.get();

    for (int i = 0; i < 3; ++i) {
        auto entry = makeEntry("msg_" + std::to_string(i));
        ptr->write(entry);
    }

    EXPECT_EQ(ptr->batchCount(), 1u);
    auto batches = ptr->batches();
    ASSERT_EQ(batches[0].size(), 3u);
    EXPECT_EQ(batches[0][0], "msg_0");
    EXPECT_EQ(batches[0][1], "msg_1");
    EXPECT_EQ(batches[0][2], "msg_2");
}

// --- Test 2: flushInterval triggers automatic flush ---
TEST_F(BatchedSinkTest, FlushIntervalTriggersAutoFlush) {
    minta::BatchOptions opts;
    opts.setBatchSize(1000).setFlushIntervalMs(200);
    auto sink = minta::detail::make_unique<MockBatchedSink>(opts);
    auto* ptr = sink.get();

    auto entry = makeEntry("interval_msg");
    ptr->write(entry);

    // Wait for timer to trigger
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_GE(ptr->batchCount(), 1u);
    EXPECT_EQ(ptr->totalEntries(), 1u);
}

// --- Test 3: Retry on writeBatch failure ---
TEST_F(BatchedSinkTest, RetryOnFailure) {
    minta::BatchOptions opts;
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(3).setRetryDelayMs(10);
    auto sink = minta::detail::make_unique<MockBatchedSink>(opts);
    auto* ptr = sink.get();

    // Throw first 2 times, succeed on 3rd (retry index 2)
    ptr->setThrowCount(2);

    auto entry = makeEntry("retry_msg");
    ptr->write(entry);

    EXPECT_EQ(ptr->batchCount(), 1u);
    auto errors = ptr->errors();
    EXPECT_EQ(errors.size(), 2u);
}

// --- Test 4: Graceful shutdown flushes remaining ---
TEST_F(BatchedSinkTest, GracefulShutdownFlushes) {
    minta::BatchOptions opts;
    opts.setBatchSize(100).setFlushIntervalMs(0); // won't trigger by size or timer
    MockBatchedSink sink(opts);

    for (int i = 0; i < 5; ++i) {
        auto entry = makeEntry("shutdown_" + std::to_string(i));
        sink.write(entry);
    }
    // stopAndFlush() is what the destructor invokes; idempotent.
    sink.stopAndFlush();
    EXPECT_EQ(sink.totalEntries(), 5u);
}

// --- Test 5: Concurrent writes ---
TEST_F(BatchedSinkTest, ConcurrentWrites) {
    minta::BatchOptions opts;
    opts.setBatchSize(10).setFlushIntervalMs(0);
    auto sink = std::make_shared<MockBatchedSink>(opts);
    auto* ptr = sink.get();

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([ptr, t] {
            for (int i = 0; i < 10; ++i) {
                auto entry = makeEntry("t" + std::to_string(t) + "_" + std::to_string(i));
                ptr->write(entry);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ptr->flush();
    EXPECT_EQ(ptr->totalEntries(), 40u);
}

// --- Test 6: onBatchError called with correct parameters ---
TEST_F(BatchedSinkTest, OnBatchErrorCalled) {
    minta::BatchOptions opts;
    opts.setBatchSize(1).setFlushIntervalMs(0).setMaxRetries(2).setRetryDelayMs(10);
    MockBatchedSink sink(opts);

    sink.setThrowCount(3); // fail all attempts (initial + 2 retries)

    auto entry = makeEntry("error_msg");
    sink.write(entry);

    auto errors = sink.errors();
    EXPECT_EQ(errors.size(), 3u); // initial attempt + 2 retries
    for (size_t i = 0; i < errors.size(); ++i) {
        EXPECT_EQ(errors[i].first, "Mock batch error");
        EXPECT_EQ(errors[i].second, i);
    }
}

// --- Test 7: flush() forces immediate batch delivery ---
TEST_F(BatchedSinkTest, ForceFlush) {
    minta::BatchOptions opts;
    opts.setBatchSize(100).setFlushIntervalMs(0);
    MockBatchedSink sink(opts);

    auto entry = makeEntry("force_flush_msg");
    sink.write(entry);
    EXPECT_EQ(sink.batchCount(), 0u); // not yet at batchSize

    sink.flush();
    EXPECT_EQ(sink.batchCount(), 1u);
    EXPECT_EQ(sink.totalEntries(), 1u);
}

// --- Test 8: writeBatch receives correct pointers ---
TEST_F(BatchedSinkTest, WriteBatchReceivesCorrectPointers) {
    minta::BatchOptions opts;
    opts.setBatchSize(5).setFlushIntervalMs(0);
    MockBatchedSink sink(opts);

    for (int i = 0; i < 5; ++i) {
        auto entry = makeEntry("ptr_test_" + std::to_string(i));
        sink.write(entry);
    }

    auto batches = sink.batches();
    ASSERT_EQ(batches.size(), 1u);
    ASSERT_EQ(batches[0].size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(batches[0][i], "ptr_test_" + std::to_string(i));
    }
}

// --- Test 9: onFlush called on success ---
TEST_F(BatchedSinkTest, OnFlushCalledOnSuccess) {
    minta::BatchOptions opts;
    opts.setBatchSize(2).setFlushIntervalMs(0);
    MockBatchedSink sink(opts);

    auto e1 = makeEntry("flush1");
    auto e2 = makeEntry("flush2");
    sink.write(e1);
    sink.write(e2);

    EXPECT_EQ(sink.flushCallCount(), 1u);
}

// --- Test 10: Empty buffer doesn't trigger writeBatch ---
TEST_F(BatchedSinkTest, EmptyBufferNoWriteBatch) {
    minta::BatchOptions opts;
    opts.setBatchSize(100).setFlushIntervalMs(0);
    MockBatchedSink sink(opts);

    sink.flush(); // Should not call writeBatch
    EXPECT_EQ(sink.batchCount(), 0u);
}

// --- Test 11: Concurrent writes with retry ---
TEST_F(BatchedSinkTest, ConcurrentWritesWithRetry) {
    minta::BatchOptions opts;
    opts.setBatchSize(5).setFlushIntervalMs(0).setMaxRetries(3).setRetryDelayMs(10);
    auto sink = std::make_shared<MockBatchedSink>(opts);
    auto* ptr = sink.get();

    ptr->setThrowCount(1);

    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([ptr, t] {
            for (int i = 0; i < 5; ++i) {
                auto entry = makeEntry("retry_t" + std::to_string(t) + "_" + std::to_string(i));
                ptr->write(entry);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ptr->flush();
    EXPECT_EQ(ptr->totalEntries(), 40u);
    EXPECT_GE(ptr->errors().size(), 1u);
}

// --- Test 12: droppedCount incremented on queue overflow ---
TEST_F(BatchedSinkTest, DroppedCountOnOverflow) {
    minta::BatchOptions opts;
    opts.setMaxQueueSize(5).setBatchSize(1000).setFlushIntervalMs(0);
    MockBatchedSink sink(opts);

    for (int i = 0; i < 10; ++i) {
        auto entry = makeEntry("drop_" + std::to_string(i));
        sink.write(entry);
    }

    EXPECT_EQ(sink.droppedCount(), 5u);
}
