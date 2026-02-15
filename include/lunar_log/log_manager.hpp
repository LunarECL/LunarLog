#ifndef LUNAR_LOG_MANAGER_HPP
#define LUNAR_LOG_MANAGER_HPP

#include "sink/sink_interface.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace minta {
    class LogManager {
    public:
        LogManager() : m_loggingStarted(false) {}

        // NOTE: addSink is not thread-safe after logging starts.
        // Add all sinks before any log calls are made.
        //
        // There is a TOCTOU race between the m_loggingStarted check and the
        // push_back: two threads could both pass the check concurrently. This
        // is acceptable because the documented contract requires all sinks to
        // be added before any log calls, so well-behaved callers never hit
        // this window. The atomic check is a best-effort safety net, not a
        // thread-safe gate.
        void addSink(std::unique_ptr<ISink> sink) {
            if (m_loggingStarted.load(std::memory_order_acquire)) {
                throw std::logic_error("Cannot add sinks after logging has started");
            }
            m_sinks.push_back(std::move(sink));
        }

        void log(const LogEntry &entry) {
            m_loggingStarted.store(true, std::memory_order_release);
            for (const auto &sink: m_sinks) {
                sink->write(entry);
            }
        }

    private:
        std::vector<std::unique_ptr<ISink> > m_sinks;
        std::atomic<bool> m_loggingStarted;
    };
} // namespace minta

#endif // LUNAR_LOG_MANAGER_HPP
