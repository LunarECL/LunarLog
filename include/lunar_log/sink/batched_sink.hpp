#ifndef LUNAR_LOG_BATCHED_SINK_HPP
#define LUNAR_LOG_BATCHED_SINK_HPP

#include "sink_interface.hpp"
#include "../core/log_common.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>
#include <utility>
#include <chrono>
#include <stdexcept>
#include <cstdio>

namespace minta {

    /// Configuration options for BatchedSink.
    ///
    /// Controls batch size, flush interval, queue limits, and retry behavior.
    /// All setters return `*this` for fluent chaining:
    /// @code
    ///   BatchOptions opts;
    ///   opts.setBatchSize(200).setFlushIntervalMs(3000).setMaxRetries(5);
    /// @endcode
    ///
    /// @warning When a producer thread triggers a flush (by reaching batchSize),
    ///          it blocks for up to (writeBatch duration + retryDelayMs) * (maxRetries+1).
    ///          For latency-sensitive producers, wrap in AsyncSink<BatchedSink<...>>
    ///          to offload flush work to a dedicated thread.
    struct BatchOptions {
        size_t batchSize_;         ///< Flush when buffer reaches this size
        size_t flushIntervalMs_;   ///< Periodic flush interval in ms
        size_t maxQueueSize_;      ///< Maximum buffered entries before dropping
        size_t maxRetries_;        ///< Retry count on writeBatch failure
        size_t retryDelayMs_;      ///< Delay between retries in ms

        BatchOptions()
            : batchSize_(100)
            , flushIntervalMs_(5000)
            , maxQueueSize_(10000)
            , maxRetries_(3)
            , retryDelayMs_(100) {}

        /// @note A value of 0 is clamped to 1 to prevent per-entry flushing.
        BatchOptions& setBatchSize(size_t n) { batchSize_ = (n > 0 ? n : 1); return *this; }
        BatchOptions& setFlushIntervalMs(size_t ms) { flushIntervalMs_ = ms; return *this; }
        BatchOptions& setMaxQueueSize(size_t n) { maxQueueSize_ = n; return *this; }
        BatchOptions& setMaxRetries(size_t n) { maxRetries_ = n; return *this; }
        BatchOptions& setRetryDelayMs(size_t ms) { retryDelayMs_ = ms; return *this; }
    };

    /// Base class for batched sink implementations.
    ///
    /// Buffers log entries and delivers them in batches via the protected
    /// writeBatch() method. A background timer thread ensures entries are
    /// flushed even when the batch size threshold is not reached.
    ///
    /// Subclasses must implement writeBatch() and may optionally override
    /// onFlush() and onBatchError().
    ///
    /// @warning **Latency impact on producers.** When a write() call fills
    ///          the batch, doFlush() runs on the **caller's** thread with
    ///          retries. With defaults (maxRetries=3, retryDelayMs=100ms)
    ///          a failing writeBatch blocks the producer for ~400ms+.
    ///          For latency-sensitive producers, wrap in AsyncSink:
    ///          `logger.addSink<AsyncSink<HttpSink>>(opts, httpOpts);`
    ///
    /// @note Batch delivery order across flush triggers is not guaranteed.
    ///       A timer-triggered flush and a size-triggered flush may race,
    ///       causing a later batch to be delivered before an earlier one.
    ///       Each individual batch preserves internal entry order.
    ///
    /// @code
    ///   class MyHttpSink : public BatchedSink {
    ///   public:
    ///       MyHttpSink() : BatchedSink(BatchOptions().setBatchSize(50)) {}
    ///   protected:
    ///       void writeBatch(const std::vector<const LogEntry*>& batch) override {
    ///           // Send batch over HTTP
    ///       }
    ///   };
    /// @endcode
    class BatchedSink : public ISink {
    public:
        explicit BatchedSink(BatchOptions opts = BatchOptions())
            : m_opts(opts)
            , m_running(true)
        {
            m_buffer.reserve(m_opts.batchSize_);
            startTimer();
        }

        ~BatchedSink() noexcept {
            if (m_running.load(std::memory_order_acquire)) {
                std::fprintf(stderr, "[LunarLog][BatchedSink] WARNING: subclass destructor did not call "
                                     "stopAndFlush() before ~BatchedSink(). "
                                     "Buffered entries may be silently discarded. "
                                     "Add stopAndFlush() to your subclass destructor.\n");
            }
            stopAndFlush();
        }

        /// Subclasses MUST call stopAndFlush() from their destructor to flush
        /// remaining entries while writeBatch() is still resolvable.
        void stopAndFlush() noexcept {
            bool expected = true;
            if (!m_running.compare_exchange_strong(expected, false,
                    std::memory_order_acq_rel)) {
                return; // already shut down
            }

            {
                std::lock_guard<std::mutex> lock(m_timerMutex);
                m_timerCV.notify_all();
            }

            if (m_timerThread.joinable()) {
                m_timerThread.join();
            }

            try {
                flushBuffer();
            } catch (...) {}
        }

        /// Buffer an entry. Triggers a flush if batchSize is reached.
        void write(const LogEntry& entry) final {
            if (!m_running.load(std::memory_order_acquire)) return;

            std::vector<LogEntry> toFlush;
            {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                if (m_opts.maxQueueSize_ > 0 && m_buffer.size() >= m_opts.maxQueueSize_) {
                    m_droppedCount.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
                m_buffer.push_back(detail::cloneEntry(entry));
                if (m_buffer.size() >= m_opts.batchSize_) {
                    toFlush.swap(m_buffer);
                }
            }
            if (!toFlush.empty()) {
                doFlush(std::move(toFlush));
            }
        }

        /// Force flush all buffered entries.
        void flush() override {
            std::vector<LogEntry> toFlush;
            {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                if (m_buffer.empty()) return;
                toFlush.swap(m_buffer);
            }
            doFlush(std::move(toFlush));
        }

        /// Access options (for testing).
        const BatchOptions& options() const { return m_opts; }

        /// Number of entries dropped due to maxQueueSize overflow or
        /// vtable-revert discard (subclass forgot stopAndFlush).
        size_t droppedCount() const {
            return m_droppedCount.load(std::memory_order_relaxed);
        }

    protected:
        /// Subclasses implement this to process a batch of entries.
        /// The pointers are valid only for the duration of this call.
        ///
        /// Default implementation is a no-op.  Subclass destructors MUST call
        /// stopAndFlush() before their vtable is destroyed; otherwise any
        /// remaining buffered entries will be silently discarded by this
        /// empty default.
        ///
        /// @warning Subclass MUST call stopAndFlush() from its destructor.
        ///          Failing to do so will discard buffered entries without
        ///          delivering them to writeBatch().
        /// @warning Thread-safety: writeBatch() is serialized by m_writeMutex,
        ///          so implementations need not be thread-safe. However, long-running
        ///          writeBatch calls will block other flush paths.
        virtual void writeBatch(const std::vector<const LogEntry*>& batch) {
            m_droppedCount.fetch_add(batch.size(), std::memory_order_relaxed);
        }

        /// Called after a successful flush. Override for post-flush logic.
        /// @note NOT serialized by m_writeMutex. Implementations must be
        ///       thread-safe if concurrent flushes are possible.
        virtual void onFlush() {}

        /// Called when writeBatch throws an exception.
        /// @param e The caught exception
        /// @param retryCount Current retry attempt (0-based)
        /// @note NOT serialized by m_writeMutex. Implementations must be
        ///       thread-safe if concurrent flushes are possible.
        virtual void onBatchError(const std::exception& e, size_t retryCount) {
            (void)e;
            (void)retryCount;
        }

    private:
        void startTimer() {
            if (m_opts.flushIntervalMs_ == 0) return;
            m_timerThread = std::thread(&BatchedSink::timerLoop, this);
        }

        void timerLoop() {
            while (m_running.load(std::memory_order_acquire)) {
                std::unique_lock<std::mutex> lock(m_timerMutex);
                m_timerCV.wait_for(lock,
                    std::chrono::milliseconds(m_opts.flushIntervalMs_),
                    [this] { return !m_running.load(std::memory_order_acquire); });

                if (!m_running.load(std::memory_order_acquire)) break;

                try {
                    std::vector<LogEntry> toFlush;
                    {
                        std::lock_guard<std::mutex> bufLock(m_bufferMutex);
                        if (m_buffer.empty()) continue;
                        toFlush.swap(m_buffer);
                    }
                    doFlush(std::move(toFlush));
                } catch (...) {}
            }
        }

        /// Flush a batch with retries. Does NOT hold m_bufferMutex.
        void doFlush(std::vector<LogEntry> entries) {
            if (entries.empty()) return;

            std::vector<const LogEntry*> ptrs;
            ptrs.reserve(entries.size());
            for (size_t i = 0; i < entries.size(); ++i) {
                ptrs.push_back(&entries[i]);
            }

            bool success = false;
            for (size_t attempt = 0; attempt <= m_opts.maxRetries_; ++attempt) {
                try {
                    {
                        std::lock_guard<std::mutex> wlock(m_writeMutex);
                        writeBatch(ptrs);
                    }
                    success = true;
                    break;
                } catch (const std::exception& e) {
                    try { onBatchError(e, attempt); } catch (...) {}
                    if (attempt < m_opts.maxRetries_) {
                        if (!m_running.load(std::memory_order_acquire)) break;
                        // Use timerCV so stopAndFlush() can interrupt the sleep.
                        std::unique_lock<std::mutex> lock(m_timerMutex);
                        m_timerCV.wait_for(lock,
                            std::chrono::milliseconds(m_opts.retryDelayMs_),
                            [this]{ return !m_running.load(std::memory_order_acquire); });
                        if (!m_running.load(std::memory_order_acquire)) break;
                    }
                } catch (...) {
                    break;
                }
            }

            if (success) {
                try { onFlush(); } catch (...) {}
            }
        }

        void flushBuffer() {
            std::vector<LogEntry> toFlush;
            {
                std::lock_guard<std::mutex> lock(m_bufferMutex);
                if (m_buffer.empty()) return;
                toFlush.swap(m_buffer);
            }
            doFlush(std::move(toFlush));
        }

        BatchOptions m_opts;
        std::vector<LogEntry> m_buffer;
        std::mutex m_bufferMutex;
        std::mutex m_writeMutex;
        std::thread m_timerThread;
        std::mutex m_timerMutex;
        std::condition_variable m_timerCV;
        std::atomic<bool> m_running;
        std::atomic<size_t> m_droppedCount{0};
    };

} // namespace minta

#endif // LUNAR_LOG_BATCHED_SINK_HPP
