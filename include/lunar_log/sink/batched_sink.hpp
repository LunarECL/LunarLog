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
            , retryDelayMs_(1000) {}

        BatchOptions& setBatchSize(size_t n) { batchSize_ = n; return *this; }
        BatchOptions& setFlushIntervalMs(size_t ms) { flushIntervalMs_ = ms; return *this; }
        BatchOptions& setMaxQueueSize(size_t n) { maxQueueSize_ = n; return *this; }
        BatchOptions& setMaxRetries(size_t n) { maxRetries_ = n; return *this; }
        BatchOptions& setRetryDelayMs(size_t ms) { retryDelayMs_ = ms; return *this; }
    };

    /// Abstract base class for batched sink implementations.
    ///
    /// Buffers log entries and delivers them in batches via the protected
    /// writeBatch() method. A background timer thread ensures entries are
    /// flushed even when the batch size threshold is not reached.
    ///
    /// Subclasses must implement writeBatch() and may optionally override
    /// onFlush() and onBatchError().
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

    protected:
        /// Subclasses implement this to process a batch of entries.
        /// The pointers are valid only for the duration of this call.
        ///
        /// Default implementation is a no-op.  Subclass destructors MUST call
        /// stopAndFlush() before their vtable is destroyed; otherwise any
        /// remaining buffered entries will be silently discarded by this
        /// empty default.
        virtual void writeBatch(const std::vector<const LogEntry*>& batch) {
            (void)batch;
        }

        /// Called after a successful flush. Override for post-flush logic.
        virtual void onFlush() {}

        /// Called when writeBatch throws an exception.
        /// @param e The caught exception
        /// @param retryCount Current retry attempt (0-based)
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
                    writeBatch(ptrs);
                    success = true;
                    break;
                } catch (const std::exception& e) {
                    onBatchError(e, attempt);
                    if (attempt < m_opts.maxRetries_) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(m_opts.retryDelayMs_));
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
        std::thread m_timerThread;
        std::mutex m_timerMutex;
        std::condition_variable m_timerCV;
        std::atomic<bool> m_running;
    };

} // namespace minta

#endif // LUNAR_LOG_BATCHED_SINK_HPP
