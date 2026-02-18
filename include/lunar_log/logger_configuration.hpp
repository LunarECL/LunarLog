#ifndef LUNAR_LOG_LOGGER_CONFIGURATION_HPP
#define LUNAR_LOG_LOGGER_CONFIGURATION_HPP

// This header is included internally by log_source.hpp AFTER the LunarLog
// class definition.  It must not be included directly — use lunar_log.hpp.

#include "core/log_level.hpp"
#include "core/enricher.hpp"
#include "core/sink_proxy.hpp"
#include "core/log_common.hpp"
#include "sink/sink_interface.hpp"
#include "formatter/formatter_interface.hpp"

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <memory>

namespace minta {

    class LunarLog;

    /// Fluent builder for constructing a fully-configured LunarLog instance.
    ///
    /// Usage:
    /// @code
    ///   auto log = LunarLog::configure()
    ///       .minLevel(LogLevel::DEBUG)
    ///       .writeTo<ConsoleSink>()
    ///       .writeTo<FileSink, JsonFormatter>("json-out", "app.jsonl")
    ///       .enrich(Enrichers::threadId())
    ///       .build();
    /// @endcode
    ///
    /// Each configuration call stores settings eagerly.  build() creates a
    /// LunarLog instance, applies all settings, adds all sinks, and starts
    /// the background processing thread.
    class LoggerConfiguration {
    public:
        LoggerConfiguration()
            : m_minLevel(LogLevel::INFO)
            , m_captureSourceLocation(false)
            , m_rateLimitMaxLogs(1000)
            , m_rateLimitWindowMs(1000)
            , m_templateCacheSize(128)
            , m_built(false) {}

        LoggerConfiguration(const LoggerConfiguration&) = delete;
        LoggerConfiguration& operator=(const LoggerConfiguration&) = delete;
        LoggerConfiguration(LoggerConfiguration&&) = default;
        LoggerConfiguration& operator=(LoggerConfiguration&&) = default;

        // ------------------------------------------------------------------
        //  Global configuration
        // ------------------------------------------------------------------

        LoggerConfiguration& minLevel(LogLevel level) {
            m_minLevel = level;
            return *this;
        }

        LoggerConfiguration& captureSourceLocation(bool enable) {
            m_captureSourceLocation = enable;
            return *this;
        }

        /// Maps to LunarLog::setRateLimit() in the imperative API.
        LoggerConfiguration& rateLimit(size_t maxLogs,
                                       std::chrono::milliseconds window) {
            m_rateLimitMaxLogs  = maxLogs;
            m_rateLimitWindowMs = static_cast<long long>(window.count());
            return *this;
        }

        LoggerConfiguration& templateCacheSize(size_t size) {
            m_templateCacheSize = size;
            return *this;
        }

        LoggerConfiguration& locale(const std::string& loc) {
            m_locale = loc;
            return *this;
        }

        LoggerConfiguration& enrich(EnricherFn fn) {
            m_enrichers.push_back(std::move(fn));
            return *this;
        }

        /// Add compact filter rules (space-separated, AND-combined).
        LoggerConfiguration& filter(const std::string& compact) {
            m_filterCompact.push_back(compact);
            return *this;
        }

        /// Add a DSL filter rule.
        LoggerConfiguration& filterRule(const std::string& dsl) {
            m_filterRules.push_back(dsl);
            return *this;
        }

        // ------------------------------------------------------------------
        //  writeTo — unnamed sink (auto-named "sink_0", "sink_1", ...)
        // ------------------------------------------------------------------

        /// Add an unnamed sink.  SFINAE: only viable when SinkType is
        /// constructible from Args.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            LoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            SinkRegistration reg;
            reg.hasName = false;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        /// Add an unnamed sink with a custom formatter.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            LoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            SinkRegistration reg;
            reg.hasName = false;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            reg.sink->setFormatter(detail::make_unique<FormatterType>());
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        // ------------------------------------------------------------------
        //  writeTo — named sink (user-provided name)
        // ------------------------------------------------------------------

        /// Add a named sink.
        ///
        /// SFINAE: disabled when SinkType is constructible from
        /// (const std::string&, Args...) — i.e. when the entire argument
        /// list (name included) could also be valid constructor arguments.
        /// In that case only the unnamed overload participates, preventing
        /// the sink name from being silently consumed as a ctor arg.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            LoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            SinkRegistration reg;
            reg.name    = name;
            reg.hasName = true;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        /// Add a named sink with a custom formatter.
        /// Same SFINAE guard as the non-formatter named overload above.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            LoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            SinkRegistration reg;
            reg.name    = name;
            reg.hasName = true;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<Args>(args)...);
            reg.sink->setFormatter(detail::make_unique<FormatterType>());
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        // ------------------------------------------------------------------
        //  writeTo — named sink + configuration lambda
        // ------------------------------------------------------------------

        /// Add a named sink with a post-create configuration callback.
        /// The callback receives a SinkProxy& for fluent sink configuration.
        /// SFINAE: ConfigFn must not be string-convertible (disambiguate from
        /// the name + ctor-args overload).
        template<typename SinkType, typename ConfigFn, typename... CtorArgs>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            !std::is_convertible<
                typename std::decay<ConfigFn>::type, std::string>::value &&
            std::is_constructible<SinkType, CtorArgs...>::value,
            LoggerConfiguration&
        >::type
        writeTo(const std::string& name, ConfigFn&& configure,
                CtorArgs&&... args) {
            SinkRegistration reg;
            reg.name    = name;
            reg.hasName = true;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<CtorArgs>(args)...);
            SinkProxy proxy(reg.sink.get());
            configure(proxy);
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        /// Add a named sink with a custom formatter and configuration callback.
        template<typename SinkType, typename FormatterType,
                 typename ConfigFn, typename... CtorArgs>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            !std::is_convertible<
                typename std::decay<ConfigFn>::type, std::string>::value &&
            std::is_constructible<SinkType, CtorArgs...>::value,
            LoggerConfiguration&
        >::type
        writeTo(const std::string& name, ConfigFn&& configure,
                CtorArgs&&... args) {
            SinkRegistration reg;
            reg.name    = name;
            reg.hasName = true;
            reg.sink = detail::make_unique<SinkType>(
                std::forward<CtorArgs>(args)...);
            reg.sink->setFormatter(detail::make_unique<FormatterType>());
            SinkProxy proxy(reg.sink.get());
            configure(proxy);
            m_sinks.push_back(std::move(reg));
            return *this;
        }

        // ------------------------------------------------------------------
        //  build()
        // ------------------------------------------------------------------

        /// Construct the configured LunarLog instance.
        ///
        /// If no writeTo() calls were made, the returned logger has no sinks
        /// and silently discards all messages (a "silent logger").  A warning
        /// is emitted to stderr in this case.
        ///
        /// @throws std::logic_error if called more than once.
        LunarLog build();   // defined in log_source.hpp (needs full LunarLog)

    private:
        struct SinkRegistration {
            std::string name;
            bool hasName;
            std::unique_ptr<ISink> sink;

            SinkRegistration() : hasName(false) {}
            SinkRegistration(SinkRegistration&&) = default;
            SinkRegistration& operator=(SinkRegistration&&) = default;
            SinkRegistration(const SinkRegistration&) = delete;
            SinkRegistration& operator=(const SinkRegistration&) = delete;
        };

        LogLevel    m_minLevel;
        bool        m_captureSourceLocation;
        size_t      m_rateLimitMaxLogs;
        long long   m_rateLimitWindowMs;
        size_t      m_templateCacheSize;
        std::string m_locale;
        std::vector<EnricherFn>       m_enrichers;
        std::vector<std::string>      m_filterCompact;
        std::vector<std::string>      m_filterRules;
        std::vector<SinkRegistration> m_sinks;
        bool m_built;
    };

} // namespace minta

#endif // LUNAR_LOG_LOGGER_CONFIGURATION_HPP
