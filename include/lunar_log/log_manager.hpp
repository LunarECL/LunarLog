#ifndef LUNAR_LOG_MANAGER_HPP
#define LUNAR_LOG_MANAGER_HPP

#include "sink/sink_interface.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <mutex>
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

        /// Filter pipeline: global predicate -> per-sink level -> per-sink predicate -> per-sink DSL rules.
        /// Global min level is checked by the caller (LunarLog::logInternal) before enqueuing.
        void log(const LogEntry &entry,
                 const FilterPredicate& globalFilter,
                 const std::vector<FilterRule>& globalFilterRules,
                 std::mutex& globalFilterMutex) {
            if (!m_loggingStarted.load(std::memory_order_relaxed)) {
                m_loggingStarted.store(true, std::memory_order_release);
            }

            {
                std::lock_guard<std::mutex> lock(globalFilterMutex);
                if (globalFilter && !globalFilter(entry)) return;
                for (const auto& rule : globalFilterRules) {
                    if (!rule.evaluate(entry)) return;
                }
            }

            for (const auto &sink : m_sinks) {
                if (sink->passesFilter(entry)) {
                    sink->write(entry);
                }
            }
        }

        void setSinkLevel(size_t index, LogLevel level) {
            if (index < m_sinks.size()) {
                m_sinks[index]->setMinLevel(level);
            }
        }

        void setSinkFilter(size_t index, FilterPredicate filter) {
            if (index < m_sinks.size()) {
                m_sinks[index]->setFilter(std::move(filter));
            }
        }

        void clearSinkFilter(size_t index) {
            if (index < m_sinks.size()) {
                m_sinks[index]->clearFilter();
            }
        }

        void addSinkFilterRule(size_t index, const std::string& ruleStr) {
            if (index < m_sinks.size()) {
                m_sinks[index]->addFilterRule(ruleStr);
            }
        }

        void clearSinkFilterRules(size_t index) {
            if (index < m_sinks.size()) {
                m_sinks[index]->clearFilterRules();
            }
        }

    private:
        std::vector<std::unique_ptr<ISink> > m_sinks;
        std::atomic<bool> m_loggingStarted;
    };
} // namespace minta

#endif // LUNAR_LOG_MANAGER_HPP
