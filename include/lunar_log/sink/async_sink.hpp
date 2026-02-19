#ifndef LUNAR_LOG_ASYNC_SINK_HPP
#define LUNAR_LOG_ASYNC_SINK_HPP

#include "sink_interface.hpp"
#include "../core/log_common.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <deque>
#include <memory>
#include <cassert>

namespace minta {

    /// Policy for handling queue overflow when the bounded queue is full.
    enum class OverflowPolicy {
        Block,       ///< Block the producer until space is available (backpressure)
        DropOldest,  ///< Drop the oldest item in the queue to make room
        DropNewest   ///< Drop the new item (default)
    };

    /// Configuration options for AsyncSink.
    struct AsyncOptions {
        size_t queueSize;            ///< Maximum number of entries in the queue
        OverflowPolicy overflowPolicy; ///< What to do when the queue is full
        size_t flushIntervalMs;      ///< Periodic flush interval in ms (0 = disabled)

        AsyncOptions()
            : queueSize(8192)
            , overflowPolicy(OverflowPolicy::DropNewest)
            , flushIntervalMs(0) {}
    };

namespace detail {

    /// Thread-safe bounded queue for LogEntry objects.
    /// Uses mutex + condition_variable + deque for C++11 compatibility.
    class BoundedQueue {
    public:
        explicit BoundedQueue(size_t capacity)
            : m_capacity(capacity > 0 ? capacity : 1)
            , m_stopped(false)
            , m_flushPending(false) {}

        /// Push an entry into the queue with the specified overflow policy.
        /// Returns true if the entry was enqueued, false if dropped.
        bool push(LogEntry&& entry, OverflowPolicy policy) {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stopped) return false;

            if (m_queue.size() >= m_capacity) {
                switch (policy) {
                    case OverflowPolicy::Block:
                        m_notFull.wait(lock, [this] {
                            return m_queue.size() < m_capacity || m_stopped;
                        });
                        if (m_stopped) return false;
                        break;
                    case OverflowPolicy::DropOldest:
                        m_queue.pop_front();
                        break;
                    case OverflowPolicy::DropNewest:
                        return false;
                }
            }

            m_queue.push_back(std::move(entry));
            m_notEmpty.notify_one();
            return true;
        }

        /// Drain all available entries into the output vector.
        /// Returns number of entries drained.
        size_t drain(std::vector<LogEntry>& out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            size_t count = m_queue.size();
            out.reserve(out.size() + count);
            for (size_t i = 0; i < count; ++i) {
                out.push_back(std::move(m_queue.front()));
                m_queue.pop_front();
            }
            if (count > 0) {
                m_notFull.notify_all();
            }
            return count;
        }

        /// Wait until at least one entry is available or the queue is stopped.
        /// Returns false if stopped with empty queue.
        bool waitForData() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_notEmpty.wait(lock, [this] {
                return !m_queue.empty() || m_stopped || m_flushPending.load(std::memory_order_acquire);
            });
            return !m_queue.empty() || m_flushPending.load(std::memory_order_acquire);
        }

        /// Wait until at least one entry is available or timeout expires.
        /// Returns false if timed out or stopped with empty queue.
        bool waitForData(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_notEmpty.wait_for(lock, timeout, [this] {
                return !m_queue.empty() || m_stopped || m_flushPending.load(std::memory_order_acquire);
            });
            return !m_queue.empty() || m_flushPending.load(std::memory_order_acquire);
        }

        /// Wake all waiters (used for shutdown).
        void wake() {
            m_notEmpty.notify_all();
            m_notFull.notify_all();
        }

        /// Stop the queue. All blocking operations return immediately.
        void stop() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped = true;
            m_notEmpty.notify_all();
            m_notFull.notify_all();
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        /// Set the flush-pending flag. When true, waitForData() predicates
        /// are satisfied even if the queue is empty, unblocking the consumer
        /// so it can process a flush request.
        void setFlushPending(bool v) {
            m_flushPending.store(v, std::memory_order_release);
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmpty;
        std::condition_variable m_notFull;
        std::deque<LogEntry> m_queue;
        size_t m_capacity;
        bool m_stopped;
        std::atomic<bool> m_flushPending;
    };

} // namespace detail

    /// Asynchronous sink decorator.
    ///
    /// Wraps any ISink-derived sink type with a dedicated queue and consumer
    /// thread. Log entries are enqueued by the producer and written by the
    /// consumer thread, decoupling slow sinks (file, network) from fast
    /// producers.
    ///
    /// The decorator pattern means existing sinks work unchanged:
    /// @code
    ///   // Wrap a FileSink in an async decorator
    ///   logger.addSink<AsyncSink<FileSink>>("app.log");
    ///
    ///   // With options
    ///   AsyncOptions opts;
    ///   opts.queueSize = 4096;
    ///   opts.overflowPolicy = OverflowPolicy::Block;
    ///   logger.addSink<AsyncSink<FileSink>>(opts, "app.log");
    /// @endcode
    ///
    /// Order guarantee: entries from a single producer are delivered in FIFO
    /// order. Multiple producers have no cross-thread ordering guarantee.
    template<typename SinkType>
    class AsyncSink : public ISink {
    public:
        /// Construct an AsyncSink with default options.
        /// All arguments are forwarded to the SinkType constructor.
        template<typename... Args>
        explicit AsyncSink(Args&&... args)
            : m_innerSink(detail::make_unique<SinkType>(std::forward<Args>(args)...))
            , m_queue(AsyncOptions().queueSize)
            , m_running(true)
            , m_opts()
            , m_droppedCount(0)
            , m_flushDone(false)
        {
            startConsumer();
        }

        /// Construct an AsyncSink with custom options.
        /// Remaining arguments are forwarded to the SinkType constructor.
        template<typename... Args>
        explicit AsyncSink(AsyncOptions opts, Args&&... args)
            : m_innerSink(detail::make_unique<SinkType>(std::forward<Args>(args)...))
            , m_queue(opts.queueSize)
            , m_running(true)
            , m_opts(opts)
            , m_droppedCount(0)
            , m_flushDone(false)
        {
            startConsumer();
        }

        ~AsyncSink() noexcept {
            m_running.store(false, std::memory_order_release);
            m_queue.stop();

            // Unblock any pending flush()
            {
                std::lock_guard<std::mutex> lock(m_flushMutex);
                m_flushDone = true;
            }
            m_flushCV.notify_all();

            if (m_thread.joinable()) {
                m_thread.join();
            }

            // Flush any remaining entries in the queue
            std::vector<LogEntry> remaining;
            m_queue.drain(remaining);
            for (size_t i = 0; i < remaining.size(); ++i) {
                try {
                    m_innerSink->write(remaining[i]);
                } catch (...) {}
            }
            try {
                if (m_innerSink) m_innerSink->flush();
            } catch (...) {}
        }

        /// Enqueue an entry for asynchronous writing.
        void write(const LogEntry& entry) override {
            LogEntry copy = detail::cloneEntry(entry);
            if (!m_queue.push(std::move(copy), m_opts.overflowPolicy)) {
                m_droppedCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        /// Flush: wait for the consumer to finish processing all queued entries.
        void flush() override {
            if (!m_running.load(std::memory_order_acquire)) return;

            {
                std::lock_guard<std::mutex> lock(m_flushMutex);
                m_flushRequested.store(true, std::memory_order_release);
                m_flushDone = false;
            }
            m_queue.setFlushPending(true);
            m_queue.wake();

            std::unique_lock<std::mutex> lock(m_flushMutex);
            m_flushCV.wait(lock, [this] {
                return m_flushDone || !m_running.load(std::memory_order_acquire);
            });
        }

        /// Number of entries dropped due to queue overflow.
        size_t droppedCount() const {
            return m_droppedCount.load(std::memory_order_relaxed);
        }

        /// Access the inner sink (for testing/inspection).
        SinkType* innerSink() { return m_innerSink.get(); }
        const SinkType* innerSink() const { return m_innerSink.get(); }

    private:
        void startConsumer() {
            m_thread = std::thread(&AsyncSink::consumerLoop, this);
        }

        void consumerLoop() {
            std::vector<LogEntry> batch;
            while (m_running.load(std::memory_order_acquire)) {
                batch.clear();

                if (m_opts.flushIntervalMs > 0) {
                    m_queue.waitForData(std::chrono::milliseconds(m_opts.flushIntervalMs));
                } else {
                    m_queue.waitForData();
                }

                m_queue.drain(batch);
                for (size_t i = 0; i < batch.size(); ++i) {
                    try {
                        m_innerSink->write(batch[i]);
                    } catch (...) {}
                }

                if (m_flushRequested.load(std::memory_order_acquire)) {
                    m_queue.setFlushPending(false);
                    std::vector<LogEntry> extra;
                    m_queue.drain(extra);
                    for (size_t i = 0; i < extra.size(); ++i) {
                        try {
                            m_innerSink->write(extra[i]);
                        } catch (...) {}
                    }

                    // Propagate flush to inner sink so buffered sinks
                    // (e.g. BatchedSink inside HttpSink) actually deliver.
                    try {
                        if (m_innerSink) {
                            m_innerSink->flush();
                        }
                    } catch (...) {}

                    {
                        std::lock_guard<std::mutex> lock(m_flushMutex);
                        m_flushRequested.store(false, std::memory_order_release);
                        m_flushDone = true;
                    }
                    m_flushCV.notify_all();
                }
            }

            // Final drain after stop signal
            batch.clear();
            m_queue.drain(batch);
            for (size_t i = 0; i < batch.size(); ++i) {
                try {
                    m_innerSink->write(batch[i]);
                } catch (...) {}
            }

            // Unblock any pending flush on shutdown
            {
                std::lock_guard<std::mutex> lock(m_flushMutex);
                m_flushDone = true;
            }
            m_flushCV.notify_all();
        }

        std::unique_ptr<SinkType> m_innerSink;
        detail::BoundedQueue m_queue;
        std::thread m_thread;
        std::atomic<bool> m_running;
        AsyncOptions m_opts;
        std::atomic<size_t> m_droppedCount;

        std::atomic<bool> m_flushRequested{false};
        std::mutex m_flushMutex;
        std::condition_variable m_flushCV;
        bool m_flushDone;
    };

} // namespace minta

#endif // LUNAR_LOG_ASYNC_SINK_HPP
