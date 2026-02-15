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
