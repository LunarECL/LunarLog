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
#include <functional>

namespace minta {
    class LunarLog;

    using FilterPredicate = std::function<bool(const LogEntry&)>;

    class ISink {
    public:
        ISink() : m_minLevel(LogLevel::TRACE), m_hasFilters(false) {}
        virtual ~ISink() = default;

        virtual void write(const LogEntry &entry) = 0;

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

        bool passesFilter(const LogEntry& entry) const {
            if (entry.level < m_minLevel.load(std::memory_order_relaxed)) {
                return false;
            }

            if (!m_hasFilters.load(std::memory_order_acquire)) {
                return true;
            }

            std::lock_guard<std::mutex> lock(m_filterMutex);
            if (m_filter && !m_filter(entry)) {
                return false;
            }
            for (const auto& rule : m_filterRules) {
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

        friend class LunarLog;
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
