#ifndef LUNAR_LOG_MANAGER_HPP
#define LUNAR_LOG_MANAGER_HPP

#include "sink/sink_interface.hpp"
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <string>

namespace minta {
    class LogManager {
    public:
        LogManager() : m_loggingStarted(false), m_nextAutoIndex(0) {}

        /// Move constructor — used by the builder pattern.
        /// Safe only when no logging has started (no concurrent access).
        LogManager(LogManager&& other) noexcept
            : m_sinks(std::move(other.m_sinks))
            , m_loggingStarted(other.m_loggingStarted.load(std::memory_order_relaxed))
            , m_sinkNames(std::move(other.m_sinkNames))
            , m_nextAutoIndex(other.m_nextAutoIndex)
        {
            other.m_loggingStarted.store(false, std::memory_order_relaxed);
            other.m_nextAutoIndex = 0;
        }

        LogManager& operator=(LogManager&&) = delete;
        LogManager(const LogManager&) = delete;
        LogManager& operator=(const LogManager&) = delete;

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
            // Auto-name unnamed sinks, skipping collisions with user-named sinks
            std::string autoName;
            do {
                autoName = "sink_" + std::to_string(m_nextAutoIndex++);
            } while (m_sinkNames.count(autoName) > 0);
            sink->setSinkName(autoName);
            m_sinks.push_back(std::move(sink));
            m_sinkNames[autoName] = m_sinks.size() - 1;
        }

        /// Add a named sink. Throws std::invalid_argument if name is duplicate.
        void addSink(const std::string& name, std::unique_ptr<ISink> sink) {
            if (m_loggingStarted.load(std::memory_order_acquire)) {
                throw std::logic_error("Cannot add sinks after logging has started");
            }
            if (m_sinkNames.count(name)) {
                throw std::invalid_argument("Duplicate sink name: " + name);
            }
            sink->setSinkName(name);
            m_sinks.push_back(std::move(sink));
            m_sinkNames[name] = m_sinks.size() - 1;
            m_nextAutoIndex++; // keep auto index advancing
        }

        /// Get sink index by name. Throws std::invalid_argument if not found.
        size_t getSinkIndex(const std::string& name) const {
            auto it = m_sinkNames.find(name);
            if (it == m_sinkNames.end()) {
                throw std::invalid_argument("Unknown sink name: " + name);
            }
            return it->second;
        }

        bool isLoggingStarted() const {
            return m_loggingStarted.load(std::memory_order_acquire);
        }

        /// Get sink pointer by index.
        ISink* getSink(size_t index) {
            requireValidIndex(index);
            return m_sinks[index].get();
        }

        const ISink* getSink(size_t index) const {
            requireValidIndex(index);
            return m_sinks[index].get();
        }

        /// Filter pipeline: global min level (caller) -> global predicate -> global DSL rules
        ///                -> per-sink tag routing -> per-sink min level -> per-sink predicate -> per-sink DSL rules.
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
                    if (!sink->shouldAcceptTags(entry.tags)) continue;
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

        void setSinkLocale(size_t index, const std::string& locale) {
            requireValidIndex(index);
            m_sinks[index]->setLocale(locale);
        }

    private:
        void requireValidIndex(size_t index) const {
            if (index >= m_sinks.size()) {
                throw std::out_of_range("Sink index out of range");
            }
        }

        std::vector<std::unique_ptr<ISink> > m_sinks;
        std::atomic<bool> m_loggingStarted;
        std::unordered_map<std::string, size_t> m_sinkNames;
        size_t m_nextAutoIndex;
        // NOTE: The global filter state (predicate, DSL rules, mutex) lives in
        // LunarLog and is passed into log() by reference.  A future cleanup
        // could bundle these into a GlobalFilterConfig struct owned here.
    };
} // namespace minta

#endif // LUNAR_LOG_MANAGER_HPP
