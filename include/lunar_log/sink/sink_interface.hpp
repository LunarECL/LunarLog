#ifndef LUNAR_LOG_SINK_INTERFACE_HPP
#define LUNAR_LOG_SINK_INTERFACE_HPP

#include "../core/log_entry.hpp"
#include "../core/log_level.hpp"
#include "../core/filter_rule.hpp"
#include "../formatter/formatter_interface.hpp"
#include "../transport/transport_interface.hpp"
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <set>
#include <string>
#include <functional>

namespace minta {
    class LunarLog;

    using FilterPredicate = std::function<bool(const LogEntry&)>;

    class ISink {
    public:
        ISink() : m_minLevel(LogLevel::TRACE), m_hasFilters(false), m_hasTagFilters(false) {}
        virtual ~ISink() = default;

        virtual void write(const LogEntry &entry) = 0;

        // --- Named sink support ---
        void setSinkName(const std::string& name) { m_sinkName = name; }
        const std::string& getSinkName() const { return m_sinkName; }

        // --- Tag routing ---
        void addOnlyTag(const std::string& tag) {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_onlyTags.insert(tag);
            m_hasTagFilters.store(true, std::memory_order_release);
        }
        void addExceptTag(const std::string& tag) {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_exceptTags.insert(tag);
            m_hasTagFilters.store(true, std::memory_order_release);
        }
        void clearOnlyTags() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_onlyTags.clear();
            m_hasTagFilters.store(!m_exceptTags.empty(), std::memory_order_release);
        }
        void clearExceptTags() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_exceptTags.clear();
            m_hasTagFilters.store(!m_onlyTags.empty(), std::memory_order_release);
        }
        void clearTagFilters() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_onlyTags.clear();
            m_exceptTags.clear();
            m_hasTagFilters.store(false, std::memory_order_release);
        }
        std::set<std::string> getOnlyTags() const {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            return m_onlyTags;
        }
        std::set<std::string> getExceptTags() const {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            return m_exceptTags;
        }

        /// Check if this sink should accept an entry based on tag routing.
        /// Rules:
        ///   - If sink has only() tags: accept only if entry has at least one matching tag
        ///   - If sink has except() tags: reject if entry has any matching tag
        ///   - only() takes precedence over except()
        ///   - Entries without tags go to sinks that have NO only() filter
        bool shouldAcceptTags(const std::vector<std::string>& entryTags) const {
            if (!m_hasTagFilters.load(std::memory_order_acquire)) {
                return true;
            }
            std::lock_guard<std::mutex> lock(m_tagMutex);
            if (!m_onlyTags.empty()) {
                // only() mode: entry must have at least one matching tag
                for (const auto& tag : entryTags) {
                    if (m_onlyTags.count(tag)) return true;
                }
                return false; // no matching tag
            }
            if (!m_exceptTags.empty()) {
                // except() mode: reject if entry has any matching tag
                for (const auto& tag : entryTags) {
                    if (m_exceptTags.count(tag)) return false;
                }
            }
            return true;
        }

        void setMinLevel(LogLevel level) {
            m_minLevel.store(level, std::memory_order_relaxed);
        }

        LogLevel getMinLevel() const {
            return m_minLevel.load(std::memory_order_relaxed);
        }

        /// @note The predicate is invoked on the consumer thread while holding an
        ///       internal mutex. It must be fast, non-blocking, and must NOT call
        ///       any filter-modification methods on the same sink (deadlock).
        ///       Filter predicates must capture state by value. Referenced
        ///       objects must outlive the logger.
        void setFilter(FilterPredicate filter) {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter = std::move(filter);
            m_hasFilters.store(true, std::memory_order_release);
        }

        void clearFilter() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter = nullptr;
            m_hasFilters.store(!m_filterRules.empty(), std::memory_order_release);
        }

        void addFilterRule(FilterRule rule) {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filterRules.push_back(std::move(rule));
            m_hasFilters.store(true, std::memory_order_release);
        }

        void addFilterRules(std::vector<FilterRule> rules) {
            if (rules.empty()) return;
            std::lock_guard<std::mutex> lock(m_filterMutex);
            for (size_t i = 0; i < rules.size(); ++i) {
                m_filterRules.push_back(std::move(rules[i]));
            }
            m_hasFilters.store(true, std::memory_order_release);
        }

        void addFilterRule(const std::string& ruleStr) {
            FilterRule rule = FilterRule::parse(ruleStr);
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filterRules.push_back(std::move(rule));
            m_hasFilters.store(true, std::memory_order_release);
        }

        void clearFilterRules() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filterRules.clear();
            m_hasFilters.store(static_cast<bool>(m_filter), std::memory_order_release);
        }

        void clearAllFilters() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter = nullptr;
            m_filterRules.clear();
            m_hasFilters.store(false, std::memory_order_release);
        }

        /// Set a per-sink locale for culture-specific formatting.
        /// Forwards to the sink's formatter.
        void setLocale(const std::string& locale) {
            IFormatter* fmt = formatter();
            if (fmt) fmt->setLocale(locale);
        }

        std::string getLocale() const {
            IFormatter* fmt = formatter();
            return fmt ? fmt->getLocale() : std::string();
        }

        bool passesFilter(const LogEntry& entry) const {
            if (entry.level < m_minLevel.load(std::memory_order_relaxed)) {
                return false;
            }

            if (!m_hasFilters.load(std::memory_order_acquire)) {
                return true;
            }

            FilterPredicate filterCopy;
            std::vector<FilterRule> rulesCopy;
            {
                std::lock_guard<std::mutex> lock(m_filterMutex);
                filterCopy = m_filter;
                rulesCopy = m_filterRules;
            }

            if (filterCopy && !filterCopy(entry)) {
                return false;
            }
            for (const auto& rule : rulesCopy) {
                if (!rule.evaluate(entry)) {
                    return false;
                }
            }
            return true;
        }

    protected:
        void setFormatter(std::unique_ptr<IFormatter> formatter) {
            m_formatter = std::move(formatter);
        }

        void setTransport(std::unique_ptr<ITransport> transport) {
            m_transport = std::move(transport);
        }

        IFormatter* formatter() const { return m_formatter.get(); }
        ITransport* transport() const { return m_transport.get(); }

    private:
        std::unique_ptr<IFormatter> m_formatter;
        std::unique_ptr<ITransport> m_transport;
        std::atomic<LogLevel> m_minLevel;
        std::atomic<bool> m_hasFilters;
        mutable std::mutex m_filterMutex;
        FilterPredicate m_filter;
        std::vector<FilterRule> m_filterRules;

        std::string m_sinkName;
        std::atomic<bool> m_hasTagFilters;
        mutable std::mutex m_tagMutex;
        std::set<std::string> m_onlyTags;
        std::set<std::string> m_exceptTags;

        friend class LunarLog;
        friend class SinkProxy;
    };

    class BaseSink : public ISink {
    public:
        void write(const LogEntry &entry) override {
            IFormatter* fmt = formatter();
            ITransport* tp = transport();
            if (fmt && tp) {
                tp->write(fmt->format(entry));
            }
        }
    };
} // namespace minta

#endif // LUNAR_LOG_SINK_INTERFACE_HPP
