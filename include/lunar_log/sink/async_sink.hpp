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
            , m_stopped(false) {}

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
                return !m_queue.empty() || m_stopped;
            });
            return !m_queue.empty();
        }

        /// Wait until at least one entry is available or timeout expires.
        /// Returns false if timed out or stopped with empty queue.
        bool waitForData(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_notEmpty.wait_for(lock, timeout, [this] {
                return !m_queue.empty() || m_stopped;
            });
            return !m_queue.empty();
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

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmpty;
        std::condition_variable m_notFull;
        std::deque<LogEntry> m_queue;
        size_t m_capacity;
        bool m_stopped;
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
        {
            startConsumer();
        }

        ~AsyncSink() noexcept {
            // Signal consumer to stop
            m_running.store(false, std::memory_order_release);
            m_queue.stop();

            // Wait for consumer thread to finish
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
        }

        /// Enqueue an entry for asynchronous writing.
        void write(const LogEntry& entry) override {
            // LogEntry is move-only, but ISink::write takes const ref.
            // We must create a copy of the entry to enqueue it.
            // This is done by constructing a new LogEntry with the same fields.
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
            m_queue.push(std::move(copy), m_opts.overflowPolicy);
        }

        /// Flush: drain the queue synchronously and wait for the inner sink
        /// to process all entries.
        void flush() {
            // Signal the consumer to wake up and process
            m_queue.wake();

            // Spin-wait until queue is empty
            // This is a best-effort synchronization point
            while (!m_queue.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            // Give consumer thread a moment to finish writing the last batch
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
            }

            // Final drain after stop signal
            batch.clear();
            m_queue.drain(batch);
            for (size_t i = 0; i < batch.size(); ++i) {
                try {
                    m_innerSink->write(batch[i]);
                } catch (...) {}
            }
        }

        std::unique_ptr<SinkType> m_innerSink;
        detail::BoundedQueue m_queue;
        std::thread m_thread;
        std::atomic<bool> m_running;
        AsyncOptions m_opts;
    };

} // namespace minta

#endif // LUNAR_LOG_ASYNC_SINK_HPP
