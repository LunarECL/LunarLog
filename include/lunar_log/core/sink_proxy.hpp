#ifndef LUNAR_LOG_SINK_PROXY_HPP
#define LUNAR_LOG_SINK_PROXY_HPP

#include "../sink/sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "compact_filter.hpp"

namespace minta {

    /// Fluent proxy for configuring a named sink.
    class SinkProxy {
    public:
        explicit SinkProxy(ISink* sink, bool loggingStarted = false)
            : m_sink(sink), m_loggingStarted(loggingStarted) {}

        SinkProxy& level(LogLevel lvl) {
            m_sink->setMinLevel(lvl);
            return *this;
        }

        SinkProxy& filterRule(const std::string& dsl) {
            m_sink->addFilterRule(dsl);
            return *this;
        }

        /// Add compact filter rules to this sink.
        /// Thread safety: uses ISink::addFilterRules for atomic batch addition.
        SinkProxy& filter(const std::string& compactExpr) {
            std::vector<FilterRule> rules = detail::parseCompactFilter(compactExpr);
            m_sink->addFilterRules(std::move(rules));
            return *this;
        }

        SinkProxy& locale(const std::string& loc) {
            m_sink->setLocale(loc);
            return *this;
        }

        SinkProxy& formatter(std::unique_ptr<IFormatter> f) {
            if (m_loggingStarted) {
                throw std::logic_error("Cannot change formatter after logging has started");
            }
            m_sink->setFormatter(std::move(f));
            return *this;
        }

        SinkProxy& filter(FilterPredicate pred) {
            m_sink->setFilter(std::move(pred));
            return *this;
        }

        SinkProxy& clearFilter() {
            m_sink->clearFilter();
            return *this;
        }

        SinkProxy& clearFilterRules() {
            m_sink->clearFilterRules();
            return *this;
        }

        SinkProxy& only(const std::string& tag) {
            m_sink->addOnlyTag(tag);
            return *this;
        }

        SinkProxy& except(const std::string& tag) {
            m_sink->addExceptTag(tag);
            return *this;
        }

        /// Clear predicate filter and DSL filter rules on this sink.
        /// Does NOT clear tag filters (only/except); use clearTagFilters() for those.
        SinkProxy& clearFilters() {
            m_sink->clearAllFilters();
            return *this;
        }

        SinkProxy& clearTagFilters() {
            m_sink->clearTagFilters();
            return *this;
        }

        /// Set the output template for text-based formatters.
        /// Only applies to HumanReadableFormatter. No-op for JSON/XML formatters.
        SinkProxy& outputTemplate(const std::string& templateStr) {
            IFormatter* fmt = m_sink->formatter();
            HumanReadableFormatter* hrf = dynamic_cast<HumanReadableFormatter*>(fmt);
            if (hrf) {
                hrf->setOutputTemplate(templateStr);
            }
            return *this;
        }

    private:
        ISink* m_sink;
        bool m_loggingStarted;
    };

} // namespace minta

#endif // LUNAR_LOG_SINK_PROXY_HPP
