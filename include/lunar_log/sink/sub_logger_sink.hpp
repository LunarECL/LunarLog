#ifndef LUNAR_LOG_SUB_LOGGER_SINK_HPP
#define LUNAR_LOG_SUB_LOGGER_SINK_HPP

#include "sink_interface.hpp"
#include "../core/log_entry.hpp"
#include "../core/log_common.hpp"
#include "../core/enricher.hpp"
#include "../core/compact_filter.hpp"
#include "../formatter/formatter_interface.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace minta {

    /// Configuration builder for a sub-logger pipeline.
    ///
    /// SubLoggerConfiguration mirrors a subset of LoggerConfiguration's
    /// fluent API but builds a self-contained mini-pipeline owned by a
    /// SubLoggerSink.  The sub-pipeline has its own enrichers, filters,
    /// and sinks that are fully independent from the parent logger.
    ///
    /// Usage (inside a subLogger() lambda):
    /// @code
    ///   sub.filter("ERROR+")
    ///      .enrich(Enrichers::property("pipeline", "error-alerts"))
    ///      .writeTo<FileSink>("error-log", "errors.log");
    /// @endcode
    ///
    /// Nested sub-loggers are supported via writeTo<SubLoggerSink>():
    /// @code
    ///   sub.writeTo<SubLoggerSink>([](SubLoggerConfiguration& inner) {
    ///       inner.filter("FATAL")
    ///            .writeTo<FileSink>("fatals.log");
    ///   });
    /// @endcode
    class SubLoggerConfiguration {
    public:
        SubLoggerConfiguration() = default;

        SubLoggerConfiguration(const SubLoggerConfiguration&) = delete;
        SubLoggerConfiguration& operator=(const SubLoggerConfiguration&) = delete;
        SubLoggerConfiguration(SubLoggerConfiguration&&) = default;
        SubLoggerConfiguration& operator=(SubLoggerConfiguration&&) = default;

        // ------------------------------------------------------------------
        //  Enrichers
        // ------------------------------------------------------------------

        /// Add an enricher to the sub-pipeline.
        /// Sub-logger enrichers are applied independently — they do NOT
        /// affect the parent logger or other sub-loggers.
        ///
        /// @warning Enricher lambdas must capture state by value or ensure
        ///          referenced objects outlive the logger.  Dangling
        ///          reference captures cause undefined behavior.
        SubLoggerConfiguration& enrich(EnricherFn fn) {
            m_enrichers.push_back(std::move(fn));
            return *this;
        }

        // ------------------------------------------------------------------
        //  Filters (applied to entries entering the sub-pipeline)
        // ------------------------------------------------------------------

        /// Add compact filter rules (space-separated, AND-combined).
        /// These are evaluated AFTER the parent's global filter has
        /// already passed the entry.
        SubLoggerConfiguration& filter(const std::string& compact) {
            std::vector<FilterRule> rules = detail::parseCompactFilter(compact);
            for (size_t i = 0; i < rules.size(); ++i) {
                m_filterRules.push_back(std::move(rules[i]));
            }
            return *this;
        }

        /// Add a DSL filter rule.
        SubLoggerConfiguration& filterRule(const std::string& dsl) {
            m_filterRules.push_back(FilterRule::parse(dsl));
            return *this;
        }

        /// Set a filter predicate.
        SubLoggerConfiguration& filter(FilterPredicate pred) {
            m_filterPredicate = std::move(pred);
            return *this;
        }

        /// Set the minimum log level for the sub-pipeline.
        ///
        /// This is applied as an ISink-level gate: the parent's dispatch
        /// loop calls passesFilter() BEFORE write(), so entries below
        /// this level never enter the sub-pipeline.  In contrast,
        /// filter() and filterRule() are evaluated INSIDE write() after
        /// the ISink gate has already passed the entry.
        SubLoggerConfiguration& minLevel(LogLevel level) {
            m_minLevel = level;
            m_hasMinLevel = true;
            return *this;
        }

        // ------------------------------------------------------------------
        //  writeTo — unnamed sink
        // ------------------------------------------------------------------

        /// Add an unnamed sink to the sub-pipeline.
        /// Unnamed sinks are auto-named "sub_sink_0", "sub_sink_1", etc.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            SubLoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            auto sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            sink->setSinkName(nextAutoName());
            m_sinks.push_back(std::move(sink));
            return *this;
        }

        /// Add an unnamed sink with a custom formatter.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            SubLoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            auto sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            sink->setSinkName(nextAutoName());
            m_sinks.push_back(std::move(sink));
            return *this;
        }

        // ------------------------------------------------------------------
        //  writeTo — named sink
        // ------------------------------------------------------------------

        /// Add a named sink to the sub-pipeline.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            SubLoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            auto sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            sink->setSinkName(name);
            m_sinks.push_back(std::move(sink));
            return *this;
        }

        /// Add a named sink with a custom formatter.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            SubLoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            auto sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            sink->setSinkName(name);
            m_sinks.push_back(std::move(sink));
            return *this;
        }

    private:
        friend class SubLoggerSink;

        std::string nextAutoName() {
            std::string name = "sub_sink_";
            name += std::to_string(m_sinkCounter++);
            return name;
        }

        std::vector<EnricherFn> m_enrichers;
        std::vector<FilterRule> m_filterRules;
        FilterPredicate m_filterPredicate;
        LogLevel m_minLevel = LogLevel::TRACE;
        bool m_hasMinLevel = false;
        std::vector<std::unique_ptr<ISink>> m_sinks;
        size_t m_sinkCounter = 0;
    };

    /// A special ISink that owns a mini logging pipeline.
    ///
    /// SubLoggerSink receives log entries from its parent logger and
    /// processes them through its own independent filter -> enricher ->
    /// sink chain.  This enables complex routing scenarios:
    ///
    ///   - Route ERROR+ entries to a dedicated alerting sink
    ///   - Attach sub-pipeline-specific metadata via enrichers
    ///   - Fan out to multiple sinks with independent formatting
    ///
    /// Sub-logger enrichers do NOT affect the parent pipeline -- they
    /// operate on a cloned copy of the log entry.  Sub-logger filters
    /// are evaluated after the parent's global filter has already
    /// accepted the entry.
    ///
    /// Filter layering:
    ///   1. ISink base: minLevel() and per-sink filters set via
    ///      setMinLevel()/setFilter() are checked by the parent's
    ///      dispatch loop (passesFilter()) BEFORE write() is called.
    ///   2. Sub-pipeline: filter()/filterRule()/filter(predicate) are
    ///      evaluated INSIDE write(), after the ISink gate has passed.
    ///   Both layers must pass for an entry to reach the sub-sinks.
    ///
    /// Thread safety: SubLoggerSink is thread-safe.  Enrichers and
    /// filter rules are immutable after construction.  Sub-sinks are
    /// responsible for their own thread safety (all built-in sinks are).
    /// write() and flush() may be called concurrently from different
    /// threads (e.g. consumer thread vs. caller flush); each sub-sink
    /// must handle this per the standard ISink contract.
    ///
    /// Composable with AsyncSink for non-blocking sub-pipelines:
    /// @code
    ///   .writeTo<AsyncSink<SubLoggerSink>>(subLoggerSinkInstance)
    /// @endcode
    ///
    /// @note Sub-loggers cannot reference the parent logger.
    /// @note Sub-sinks are NOT registered in the parent LogManager's
    ///       name registry.  They are owned exclusively by this sink
    ///       and not accessible via LogManager::sink(name).
    class SubLoggerSink : public ISink {
    public:
        /// Construct a SubLoggerSink from a configured SubLoggerConfiguration.
        /// Ownership of all enrichers, filters, and sinks is transferred.
        explicit SubLoggerSink(SubLoggerConfiguration config)
            : m_enrichers(std::move(config.m_enrichers))
            , m_filterRules(std::move(config.m_filterRules))
            , m_filterPredicate(std::move(config.m_filterPredicate))
            , m_sinks(std::move(config.m_sinks))
        {
            if (config.m_hasMinLevel) {
                setMinLevel(config.m_minLevel);
            }
        }

        /// Construct a SubLoggerSink from a configuration lambda.
        /// This is the typical usage path via the builder.
        template<typename ConfigFn,
                 typename = typename std::enable_if<
                     !std::is_same<typename std::decay<ConfigFn>::type,
                                   SubLoggerSink>::value &&
                     !std::is_same<typename std::decay<ConfigFn>::type,
                                   SubLoggerConfiguration>::value
                 >::type>
        explicit SubLoggerSink(ConfigFn&& configure) {
            SubLoggerConfiguration config;
            configure(config);
            m_enrichers = std::move(config.m_enrichers);
            m_filterRules = std::move(config.m_filterRules);
            m_filterPredicate = std::move(config.m_filterPredicate);
            m_sinks = std::move(config.m_sinks);
            if (config.m_hasMinLevel) {
                setMinLevel(config.m_minLevel);
            }
        }

        /// Process a log entry through the sub-pipeline.
        ///
        /// 1. Clone the entry (enrichers must not affect the parent)
        /// 2. Apply sub-pipeline filters (predicate + DSL rules)
        /// 3. Apply sub-pipeline enrichers
        /// 4. Dispatch to all sub-sinks (each applies its own per-sink filters)
        void write(const LogEntry& entry) override {
            // Step 1: Apply sub-pipeline-level filters on the original entry
            //         (before cloning, to avoid unnecessary copies)
            if (m_filterPredicate) {
                try {
                    if (!m_filterPredicate(entry)) return;
                } catch (...) {
                    // Fail-open: let the entry through on filter error
                }
            }
            for (size_t i = 0; i < m_filterRules.size(); ++i) {
                try {
                    if (!m_filterRules[i].evaluate(entry)) return;
                } catch (...) {}
            }

            // Step 2: Clone the entry so enrichers don't affect parent
            LogEntry clone = detail::cloneEntry(entry);

            // Step 3: Apply sub-pipeline enrichers
            for (size_t i = 0; i < m_enrichers.size(); ++i) {
                try {
                    m_enrichers[i](clone);
                } catch (...) {}
            }

            // Step 4: Dispatch to sub-sinks with per-sink filtering
            for (const auto& sink : m_sinks) {
                try {
                    if (!sink->shouldAcceptTags(clone.tags)) continue;
                    if (sink->passesFilter(clone)) {
                        sink->write(clone);
                    }
                } catch (...) {
                    // One bad sub-sink must not prevent others from running
                }
            }
        }

        /// Flush all sub-sinks.
        void flush() override {
            for (const auto& sink : m_sinks) {
                try {
                    sink->flush();
                } catch (...) {}
            }
        }

        /// Access sub-sinks for testing/inspection.
        size_t sinkCount() const { return m_sinks.size(); }

        ISink* subSink(size_t index) {
            return index < m_sinks.size() ? m_sinks[index].get() : nullptr;
        }
        const ISink* subSink(size_t index) const {
            return index < m_sinks.size() ? m_sinks[index].get() : nullptr;
        }

        /// Access enricher count for testing/inspection.
        size_t enricherCount() const { return m_enrichers.size(); }

        /// Access filter rule count for testing/inspection.
        size_t filterRuleCount() const { return m_filterRules.size(); }

    private:
        std::vector<EnricherFn> m_enrichers;
        std::vector<FilterRule> m_filterRules;
        FilterPredicate m_filterPredicate;
        std::vector<std::unique_ptr<ISink>> m_sinks;
    };

} // namespace minta

#endif // LUNAR_LOG_SUB_LOGGER_SINK_HPP
