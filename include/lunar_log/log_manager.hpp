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

        /// Filter pipeline: global min level (caller) -> global predicate -> global DSL rules
        ///                -> per-sink min level -> per-sink predicate -> per-sink DSL rules.
        void log(const LogEntry &entry,
                 const FilterPredicate& globalFilter,
                 const std::vector<FilterRule>& globalFilterRules,
                 std::mutex& globalFilterMutex,
                 const std::atomic<bool>& hasGlobalFilters) {
            if (!m_loggingStarted.load(std::memory_order_relaxed)) {
                m_loggingStarted.store(true, std::memory_order_release);
            }

            if (hasGlobalFilters.load(std::memory_order_acquire)) {
                try {
                    std::lock_guard<std::mutex> lock(globalFilterMutex);
                    if (globalFilter && !globalFilter(entry)) return;
                    for (const auto& rule : globalFilterRules) {
                        if (!rule.evaluate(entry)) return;
                    }
                } catch (...) {
                    // Bad global filter — fail-open: let the entry through
                    // rather than silently dropping it for all sinks.
                }
            }

            for (const auto &sink : m_sinks) {
                try {
                    if (sink->passesFilter(entry)) {
                        sink->write(entry);
                    }
                } catch (...) {
                    // One bad sink must not prevent subsequent sinks from running.
                }
            }
        }

        void setSinkLevel(size_t index, LogLevel level) {
            requireValidIndex(index);
            m_sinks[index]->setMinLevel(level);
        }

        void setSinkFilter(size_t index, FilterPredicate filter) {
            requireValidIndex(index);
            m_sinks[index]->setFilter(std::move(filter));
        }

        void clearSinkFilter(size_t index) {
            requireValidIndex(index);
            m_sinks[index]->clearFilter();
        }

        void addSinkFilterRule(size_t index, const std::string& ruleStr) {
            requireValidIndex(index);
            m_sinks[index]->addFilterRule(ruleStr);
        }

        void clearSinkFilterRules(size_t index) {
            requireValidIndex(index);
            m_sinks[index]->clearFilterRules();
        }

        void clearAllSinkFilters(size_t index) {
            requireValidIndex(index);
            m_sinks[index]->clearAllFilters();
        }

    private:
        void requireValidIndex(size_t index) const {
            if (index >= m_sinks.size()) {
                throw std::out_of_range("Sink index out of range");
            }
        }

        std::vector<std::unique_ptr<ISink> > m_sinks;
        std::atomic<bool> m_loggingStarted;
        // NOTE: The global filter state (predicate, DSL rules, mutex) lives in
        // LunarLog and is passed into log() by reference.  A future cleanup
        // could bundle these into a GlobalFilterConfig struct owned here.
    };
} // namespace minta

#endif // LUNAR_LOG_MANAGER_HPP
