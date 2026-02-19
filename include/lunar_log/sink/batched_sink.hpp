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
            std::lock_guard<std::mutex> lock(m_bufferMutex);

            // Drop if buffer exceeds max queue size
            if (m_opts.maxQueueSize_ > 0 && m_buffer.size() >= m_opts.maxQueueSize_) {
                return;
            }

            // Copy the entry into the buffer (LogEntry is move-only,
            // but write() receives a const ref)
            LogEntry copy(
                entry.level,
                std::string(entry.message),
                entry.timestamp,
                std::string(entry.templateStr),
                entry.templateHash,
                std::vector<std::pair<std::string, std::string>>(entry.arguments),
                std::string(entry.file),
                entry.line,
                std::string(entry.function),
                std::map<std::string, std::string>(entry.customContext),
                std::vector<PlaceholderProperty>(entry.properties),
                std::vector<std::string>(entry.tags),
                std::string(entry.locale),
                entry.threadId
            );
            if (entry.exception) {
                std::unique_ptr<detail::ExceptionInfo> exCopy(new detail::ExceptionInfo());
                exCopy->type = entry.exception->type;
                exCopy->message = entry.exception->message;
                exCopy->chain = entry.exception->chain;
                copy.exception = std::move(exCopy);
            }
            m_buffer.push_back(std::move(copy));

            if (m_buffer.size() >= m_opts.batchSize_) {
                flushBufferLocked();
            }
        }

        /// Force flush all buffered entries.
        void flush() {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            flushBufferLocked();
        }

        /// Access options (for testing).
        const BatchOptions& options() const { return m_opts; }

    protected:
        /// Subclasses implement this to process a batch of entries.
        /// The pointers are valid only for the duration of this call.
        virtual void writeBatch(const std::vector<const LogEntry*>& batch) = 0;

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
                    std::lock_guard<std::mutex> bufLock(m_bufferMutex);
                    flushBufferLocked();
                } catch (...) {}
            }
        }

        /// Flush without acquiring m_bufferMutex (caller must hold it).
        void flushBufferLocked() {
            if (m_buffer.empty()) return;

            // Build pointer vector for writeBatch
            std::vector<const LogEntry*> ptrs;
            ptrs.reserve(m_buffer.size());
            for (size_t i = 0; i < m_buffer.size(); ++i) {
                ptrs.push_back(&m_buffer[i]);
            }

            // Try with retries
            bool success = false;
            for (size_t attempt = 0; attempt <= m_opts.maxRetries_; ++attempt) {
                try {
                    writeBatch(ptrs);
                    success = true;
                    break;
                } catch (const std::exception& e) {
                    onBatchError(e, attempt);
                    if (attempt < m_opts.maxRetries_) {
                        // Release buffer lock during sleep to avoid blocking writes
                        // But we can't release it here since we need the buffer intact.
                        // Instead, just sleep briefly.
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(m_opts.retryDelayMs_));
                    }
                } catch (...) {
                    // Non-std::exception — no retry
                    break;
                }
            }

            // Clear buffer regardless of success (avoid infinite retry loops)
            m_buffer.clear();

            if (success) {
                try { onFlush(); } catch (...) {}
            }
        }

        /// Non-locked flush (acquires m_bufferMutex).
        void flushBuffer() {
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            flushBufferLocked();
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
