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

        // --- Tag routing (COW) ---
        void addOnlyTag(const std::string& tag) {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            auto newTags = std::make_shared<std::set<std::string>>(
                m_onlyTags ? *m_onlyTags : std::set<std::string>());
            newTags->insert(tag);
            m_onlyTags = std::move(newTags);
            m_hasTagFilters.store(true, std::memory_order_release);
        }
        void addExceptTag(const std::string& tag) {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            auto newTags = std::make_shared<std::set<std::string>>(
                m_exceptTags ? *m_exceptTags : std::set<std::string>());
            newTags->insert(tag);
            m_exceptTags = std::move(newTags);
            m_hasTagFilters.store(true, std::memory_order_release);
        }
        void clearOnlyTags() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_onlyTags.reset();
            m_hasTagFilters.store(m_exceptTags && !m_exceptTags->empty(), std::memory_order_release);
        }
        void clearExceptTags() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_exceptTags.reset();
            m_hasTagFilters.store(m_onlyTags && !m_onlyTags->empty(), std::memory_order_release);
        }
        void clearTagFilters() {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            m_onlyTags.reset();
            m_exceptTags.reset();
            m_hasTagFilters.store(false, std::memory_order_release);
        }
        std::set<std::string> getOnlyTags() const {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            return m_onlyTags ? *m_onlyTags : std::set<std::string>();
        }
        std::set<std::string> getExceptTags() const {
            std::lock_guard<std::mutex> lock(m_tagMutex);
            return m_exceptTags ? *m_exceptTags : std::set<std::string>();
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
            std::shared_ptr<const std::set<std::string>> onlyTags;
            std::shared_ptr<const std::set<std::string>> exceptTags;
            {
                std::lock_guard<std::mutex> lock(m_tagMutex);
                onlyTags = m_onlyTags;
                exceptTags = m_exceptTags;
            }
            if (onlyTags && !onlyTags->empty()) {
                for (const auto& tag : entryTags) {
                    if (onlyTags->count(tag)) return true;
                }
                return false;
            }
            if (exceptTags && !exceptTags->empty()) {
                for (const auto& tag : entryTags) {
                    if (exceptTags->count(tag)) return false;
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

        /// @note The predicate is invoked on the consumer thread against an
        ///       immutable snapshot.  It must be fast and non-blocking.
        ///       Filter predicates must capture state by value.  Referenced
        ///       objects must outlive the logger.
        void setFilter(FilterPredicate filter) {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter = std::make_shared<const FilterPredicate>(std::move(filter));
            m_hasFilters.store(true, std::memory_order_release);
        }

        void clearFilter() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter.reset();
            m_hasFilters.store(m_filterRules && !m_filterRules->empty(), std::memory_order_release);
        }

        void addFilterRule(FilterRule rule) {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            auto newRules = std::make_shared<std::vector<FilterRule>>(
                m_filterRules ? *m_filterRules : std::vector<FilterRule>());
            newRules->push_back(std::move(rule));
            m_filterRules = std::move(newRules);
            m_hasFilters.store(true, std::memory_order_release);
        }

        void addFilterRules(std::vector<FilterRule> rules) {
            if (rules.empty()) return;
            std::lock_guard<std::mutex> lock(m_filterMutex);
            auto newRules = std::make_shared<std::vector<FilterRule>>(
                m_filterRules ? *m_filterRules : std::vector<FilterRule>());
            newRules->reserve(newRules->size() + rules.size());
            for (size_t i = 0; i < rules.size(); ++i) {
                newRules->push_back(std::move(rules[i]));
            }
            m_filterRules = std::move(newRules);
            m_hasFilters.store(true, std::memory_order_release);
        }

        void addFilterRule(const std::string& ruleStr) {
            FilterRule rule = FilterRule::parse(ruleStr);
            std::lock_guard<std::mutex> lock(m_filterMutex);
            auto newRules = std::make_shared<std::vector<FilterRule>>(
                m_filterRules ? *m_filterRules : std::vector<FilterRule>());
            newRules->push_back(std::move(rule));
            m_filterRules = std::move(newRules);
            m_hasFilters.store(true, std::memory_order_release);
        }

        void clearFilterRules() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filterRules.reset();
            m_hasFilters.store(m_filter && static_cast<bool>(*m_filter), std::memory_order_release);
        }

        void clearAllFilters() {
            std::lock_guard<std::mutex> lock(m_filterMutex);
            m_filter.reset();
            m_filterRules.reset();
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

            std::shared_ptr<const FilterPredicate> filter;
            std::shared_ptr<const std::vector<FilterRule>> rules;
            {
                std::lock_guard<std::mutex> lock(m_filterMutex);
                filter = m_filter;
                rules = m_filterRules;
            }

            if (filter && *filter && !(*filter)(entry)) {
                return false;
            }
            if (rules) {
                for (const auto& rule : *rules) {
                    if (!rule.evaluate(entry)) {
                        return false;
                    }
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
        std::shared_ptr<const FilterPredicate> m_filter;
        std::shared_ptr<const std::vector<FilterRule>> m_filterRules;

        std::string m_sinkName;
        std::atomic<bool> m_hasTagFilters;
        mutable std::mutex m_tagMutex;
        std::shared_ptr<const std::set<std::string>> m_onlyTags;
        std::shared_ptr<const std::set<std::string>> m_exceptTags;

        friend class LunarLog;
        friend class LoggerConfiguration;
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
