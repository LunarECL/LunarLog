#ifndef LUNAR_LOG_SOURCE_HPP
#define LUNAR_LOG_SOURCE_HPP

#include "core/log_entry.hpp"
#include "core/log_common.hpp"
#include "core/filter_rule.hpp"
#include "core/compact_filter.hpp"
#include "core/enricher.hpp"
#include "core/exception_info.hpp"
#include "core/sink_proxy.hpp"
#include "logger_configuration.hpp"
#include "log_manager.hpp"
#include "sink/console_sink.hpp"
#include "formatter/human_readable_formatter.hpp"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <set>
#include <functional>
#include <map>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <climits>
#include <cmath>
#include <sstream>
#include <list>

namespace minta {

    /// Tag type for disambiguating named-sink overloads from unnamed ones (H3).
    /// Use: logger.addSink<ConsoleSink>(named("console"))
    /// This prevents SFINAE ambiguity when SinkType is constructible from std::string.
    struct SinkName {
        std::string value;
        explicit SinkName(const std::string& n) : value(n) {}
        explicit SinkName(const char* n) : value(n) {}
    };

    /// Convenience factory for SinkName.
    inline SinkName named(const std::string& name) { return SinkName(name); }
    inline SinkName named(const char* name) { return SinkName(name); }

namespace detail {
    /// Parse [bracketed] tag prefixes from a message template.
    /// Tags must be at the start, contain only alphanumeric, hyphens, underscores.
    /// Brackets must be immediately adjacent: "[a][b] msg" parses two tags,
    /// but "[a] [b] msg" only parses one because the space breaks the scan.
    /// Returns the tags and the remaining message (stripped of tag prefixes).
    inline std::pair<std::vector<std::string>, std::string> parseTags(const std::string& messageTemplate) {
        std::vector<std::string> tags;
        size_t pos = 0;
        while (pos < messageTemplate.size() && messageTemplate[pos] == '[') {
            size_t close = messageTemplate.find(']', pos + 1);
            if (close == std::string::npos) break;
            std::string tag = messageTemplate.substr(pos + 1, close - pos - 1);
            // Validate tag: alphanumeric + hyphens + underscores only
            bool valid = !tag.empty();
            for (size_t i = 0; i < tag.size() && valid; ++i) {
                char c = tag[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '_')) {
                    valid = false;
                }
            }
            if (!valid) break;
            tags.push_back(std::move(tag));
            pos = close + 1;
        }
        // Fast path: no tags found — pair construction still copies
        // messageTemplate, but avoids the substr + whitespace-strip work.
        if (tags.empty()) {
            return {tags, messageTemplate};
        }
        // Strip leading whitespace (spaces and tabs) after tags
        while (pos < messageTemplate.size() && (messageTemplate[pos] == ' ' || messageTemplate[pos] == '\t')) ++pos;
        return {tags, messageTemplate.substr(pos)};
    }
    using ScopeFrame = std::vector<std::pair<std::string, std::string>>;
    using ScopeStack = std::list<ScopeFrame>;
    using ScopeFrameIter = ScopeStack::iterator;

    inline ScopeStack& threadScopeStack() {
        thread_local ScopeStack stack;
        return stack;
    }

} // namespace detail

    class LoggerConfiguration;
    class LogScope;

    class LunarLog {
    public:
        explicit LunarLog(LogLevel minLevel = LogLevel::INFO, bool addDefaultConsoleSink = true)
            : m_minLevel(minLevel)
            , m_isRunning(true)
            , m_rateLimitWindowStart(std::chrono::steady_clock::now().time_since_epoch().count())
            , m_logCount(0)
            , m_captureSourceLocation(false)
            , m_hasCustomContext(false)
            , m_sinkWriteInProgress(false)
            , m_templateCacheSize(128)
            , m_hasGlobalFilters(false)
            , m_rateLimitMaxLogs(1000)
            , m_rateLimitWindowMs(1000)
            , m_threadStarted(false) {
            if (addDefaultConsoleSink) {
                addSink<ConsoleSink>();
            }
            ensureProcessingThread();
        }

        // NOTE: LunarLog must outlive all logging threads. Destroying LunarLog
        // while other threads are still calling log methods is undefined behavior.
        ~LunarLog() noexcept {
            if (m_threadStarted.load(std::memory_order_acquire)) {
                flush();
                m_isRunning = false;
                m_logCV.notify_one();
                if (m_logThread.joinable()) {
                    m_logThread.join();
                }
            }
        }

        LunarLog(const LunarLog &) = delete;
        LunarLog &operator=(const LunarLog &) = delete;

        /// Move constructor — only safe before logging starts (used by builder).
        LunarLog(LunarLog &&other) noexcept
            : m_minLevel(other.m_minLevel.load(std::memory_order_relaxed))
            , m_isRunning(true)
            , m_rateLimitWindowStart(other.m_rateLimitWindowStart.load(std::memory_order_relaxed))
            , m_logCount(0)
            , m_logManager(std::move(other.m_logManager))
            , m_customContext(std::move(other.m_customContext))
            , m_captureSourceLocation(other.m_captureSourceLocation.load(std::memory_order_relaxed))
            , m_hasCustomContext(other.m_hasCustomContext.load(std::memory_order_relaxed))
            , m_sinkWriteInProgress(false)
            , m_templateCache(std::move(other.m_templateCache))
            , m_templateCacheSize(other.m_templateCacheSize)
            , m_hasGlobalFilters(other.m_hasGlobalFilters.load(std::memory_order_relaxed))
            , m_globalFilter(std::move(other.m_globalFilter))
            , m_globalFilterRules(std::move(other.m_globalFilterRules))
            , m_hasLocale(other.m_hasLocale.load(std::memory_order_relaxed))
            , m_locale(std::move(other.m_locale))
            , m_enrichers(std::move(other.m_enrichers))
            , m_hasEnrichers(other.m_hasEnrichers.load(std::memory_order_relaxed))
            , m_rateLimitMaxLogs(other.m_rateLimitMaxLogs)
            , m_rateLimitWindowMs(other.m_rateLimitWindowMs)
            , m_threadStarted(false)
        {
            other.m_isRunning.store(false, std::memory_order_relaxed);
        }

        LunarLog &operator=(LunarLog &&) = delete;

        /// Create a fluent builder for configuring a new LunarLog instance.
        static LoggerConfiguration configure();

        void setMinLevel(LogLevel level) {
            m_minLevel.store(level, std::memory_order_relaxed);
        }

        LogLevel getMinLevel() const {
            return m_minLevel.load(std::memory_order_relaxed);
        }

        void setCaptureSourceLocation(bool capture) {
            m_captureSourceLocation.store(capture, std::memory_order_relaxed);
        }

        bool getCaptureSourceLocation() const {
            return m_captureSourceLocation.load(std::memory_order_relaxed);
        }

        /// Set rate-limit parameters (max messages per window).
        /// Must be called during setup, before the first log call.
        /// Corresponds to LoggerConfiguration::rateLimit() in the builder API.
        void setRateLimit(size_t maxLogs, std::chrono::milliseconds window) {
            m_rateLimitMaxLogs = maxLogs;
            m_rateLimitWindowMs = static_cast<long long>(window.count());
        }

        /// @deprecated Use setCaptureSourceLocation instead.
        inline void setCaptureContext(bool capture) {
            setCaptureSourceLocation(capture);
        }

        /// @deprecated Use getCaptureSourceLocation instead.
        inline bool getCaptureContext() const {
            return getCaptureSourceLocation();
        }

        /// @warning Do not call flush() from within a sink write() implementation.
        ///          flush() acquires the queue mutex and waits for sink writes to
        ///          complete, so calling it from inside write() will deadlock.
        void flush() {
            if (!m_threadStarted.load(std::memory_order_acquire)) return;
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_flushCV.wait(lock, [this] {
                return m_logQueue.empty() && !m_sinkWriteInProgress.load(std::memory_order_relaxed);
            });
        }

        /// Add an unnamed sink (auto-named "sink_0", "sink_1", etc.).
        /// SFINAE: only viable when SinkType is constructible from Args.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(Args &&... args) {
            m_logManager.addSink(detail::make_unique<SinkType>(std::forward<Args>(args)...));
        }

        /// Add an unnamed sink with a custom formatter.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(Args &&... args) {
            auto sink = detail::make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            m_logManager.addSink(std::move(sink));
        }

        /// Add a named sink: addSink<ConsoleSink>(named("console"))
        /// Uses SinkName tag type to avoid SFINAE ambiguity with unnamed overloads.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(const SinkName& sinkName, Args &&... args) {
            m_logManager.addSink(sinkName.value, detail::make_unique<SinkType>(std::forward<Args>(args)...));
        }

        /// Add a named sink with a custom formatter:
        /// addSink<FileSink, JsonFormatter>(named("json-out"), "app.jsonl")
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(const SinkName& sinkName, Args &&... args) {
            auto sink = detail::make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            m_logManager.addSink(sinkName.value, std::move(sink));
        }

        /// Convenience: addSink<SinkType>("name", args...) still works via
        /// const std::string& overload. This is unambiguous because SinkName
        /// is explicit-only from string, so implicit conversions go here.
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(const std::string& name, Args &&... args) {
            m_logManager.addSink(name, detail::make_unique<SinkType>(std::forward<Args>(args)...));
        }

        /// Named sink with formatter via string name.
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value
        >::type
        addSink(const std::string& name, Args &&... args) {
            auto sink = detail::make_unique<SinkType>(std::forward<Args>(args)...);
            sink->setFormatter(detail::make_unique<FormatterType>());
            m_logManager.addSink(name, std::move(sink));
        }

        void addCustomSink(std::unique_ptr<ISink> sink) {
            m_logManager.addSink(std::move(sink));
        }

        void addCustomSink(const std::string& name, std::unique_ptr<ISink> sink) {
            m_logManager.addSink(name, std::move(sink));
        }

        /// Get a SinkProxy for configuring a named sink.
        /// Throws std::invalid_argument if name is not found.
        SinkProxy sink(const std::string& name);

        template<typename... Args>
        void log(LogLevel level, const std::string &messageTemplate, const Args &... args) {
            logInternal(level, "", 0, "", messageTemplate, args...);
        }

        template<typename... Args>
        void logWithSourceLocation(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            logInternal(level, file, line, function, messageTemplate, args...);
        }

        /// @deprecated Use logWithSourceLocation instead.
        template<typename... Args>
        void logWithContext(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            logWithSourceLocation(level, file, line, function, messageTemplate, args...);
        }

        template<typename... Args>
        void logWithSourceLocationAndException(LogLevel level, const char* file, int line, const char* function,
                                               const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            logInternalWithException(level, file, line, function, ex, messageTemplate, args...);
        }

        template<typename... Args>
        void trace(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::TRACE, messageTemplate, args...);
        }

        template<typename... Args>
        void debug(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::DEBUG, messageTemplate, args...);
        }

        template<typename... Args>
        void info(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::INFO, messageTemplate, args...);
        }

        template<typename... Args>
        void warn(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::WARN, messageTemplate, args...);
        }

        template<typename... Args>
        void error(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::ERROR, messageTemplate, args...);
        }

        template<typename... Args>
        void fatal(const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::FATAL, messageTemplate, args...);
        }

        template<typename... Args>
        void log(LogLevel level, const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            logInternalWithException(level, "", 0, "", ex, messageTemplate, args...);
        }

        void log(LogLevel level, const std::exception& ex) {
            logInternalWithException(level, "", 0, "", ex, detail::safeWhat(ex));
        }

        template<typename... Args>
        void trace(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::TRACE, ex, messageTemplate, args...);
        }
        void trace(const std::exception& ex) { log(LogLevel::TRACE, ex); }

        template<typename... Args>
        void debug(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::DEBUG, ex, messageTemplate, args...);
        }
        void debug(const std::exception& ex) { log(LogLevel::DEBUG, ex); }

        template<typename... Args>
        void info(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::INFO, ex, messageTemplate, args...);
        }
        void info(const std::exception& ex) { log(LogLevel::INFO, ex); }

        template<typename... Args>
        void warn(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::WARN, ex, messageTemplate, args...);
        }
        void warn(const std::exception& ex) { log(LogLevel::WARN, ex); }

        template<typename... Args>
        void error(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::ERROR, ex, messageTemplate, args...);
        }
        void error(const std::exception& ex) { log(LogLevel::ERROR, ex); }

        template<typename... Args>
        void fatal(const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            log(LogLevel::FATAL, ex, messageTemplate, args...);
        }
        void fatal(const std::exception& ex) { log(LogLevel::FATAL, ex); }

        /// @note The predicate is invoked on the consumer thread while holding an
        ///       internal mutex. It must be fast, non-blocking, and must NOT call
        ///       any filter-modification methods on the same logger (deadlock).
        ///       Filter predicates must capture state by value. Referenced
        ///       objects must outlive the logger.
        ///       If the global predicate throws, the entry is let through
        ///       (fail-open) rather than silently dropped for all sinks.
        void setFilter(FilterPredicate filter) {
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            m_globalFilter = std::move(filter);
            m_hasGlobalFilters.store(true, std::memory_order_release);
        }

        void clearFilter() {
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            m_globalFilter = nullptr;
            m_hasGlobalFilters.store(!m_globalFilterRules.empty(), std::memory_order_release);
        }

        void addFilterRule(const std::string& ruleStr) {
            FilterRule rule = FilterRule::parse(ruleStr);
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            m_globalFilterRules.push_back(std::move(rule));
            m_hasGlobalFilters.store(true, std::memory_order_release);
        }

        void clearFilterRules() {
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            m_globalFilterRules.clear();
            m_hasGlobalFilters.store(static_cast<bool>(m_globalFilter), std::memory_order_release);
        }

        void clearAllFilters() {
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            m_globalFilter = nullptr;
            m_globalFilterRules.clear();
            m_hasGlobalFilters.store(false, std::memory_order_release);
        }

        /// Add compact filter rules (space-separated, AND-combined).
        /// Syntax: "WARN+", "~keyword", "!~keyword", "ctx:key", "ctx:key=val",
        /// "tpl:pattern", "!tpl:pattern". See Compact-Filter wiki page.
        /// Thread-safe — acquires global filter mutex.
        void filter(const std::string& compactExpr) {
            std::vector<FilterRule> rules = detail::parseCompactFilter(compactExpr);
            std::lock_guard<std::mutex> lock(m_globalFilterMutex);
            for (size_t i = 0; i < rules.size(); ++i) {
                m_globalFilterRules.push_back(std::move(rules[i]));
            }
            if (!m_globalFilterRules.empty()) {
                m_hasGlobalFilters.store(true, std::memory_order_release);
            }
        }

        void setSinkLevel(size_t sinkIndex, LogLevel level) {
            m_logManager.setSinkLevel(sinkIndex, level);
        }

        void setSinkFilter(size_t sinkIndex, FilterPredicate filter) {
            m_logManager.setSinkFilter(sinkIndex, std::move(filter));
        }

        void clearSinkFilter(size_t sinkIndex) {
            m_logManager.clearSinkFilter(sinkIndex);
        }

        void addSinkFilterRule(size_t sinkIndex, const std::string& ruleStr) {
            m_logManager.addSinkFilterRule(sinkIndex, ruleStr);
        }

        void clearSinkFilterRules(size_t sinkIndex) {
            m_logManager.clearSinkFilterRules(sinkIndex);
        }

        void clearAllSinkFilters(size_t sinkIndex) {
            m_logManager.clearAllSinkFilters(sinkIndex);
        }

        void setContext(const std::string& key, const std::string& value) {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext[key] = value;
            m_hasCustomContext.store(true, std::memory_order_release);
        }

        void clearContext(const std::string& key) {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.erase(key);
            m_hasCustomContext.store(!m_customContext.empty(), std::memory_order_release);
        }

        void clearAllContext() {
            std::lock_guard<std::mutex> lock(m_contextMutex);
            m_customContext.clear();
            m_hasCustomContext.store(false, std::memory_order_release);
        }

        LogScope scope(std::initializer_list<std::pair<std::string, std::string>> pairs);

        /// Set the maximum number of cached template parse results.
        /// Setting to 0 disables caching and clears existing entries.
        /// Shrinking to a non-zero value does NOT trim existing entries —
        /// they remain accessible for lookups but no new entries are
        /// inserted until the map size drops below the new cap.
        void setTemplateCacheSize(size_t size) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_templateCacheSize = size;
            if (size == 0) {
                m_templateCache.clear();
            }
        }

        void setLocale(const std::string& locale) {
            std::lock_guard<std::mutex> lock(m_localeMutex);
            m_locale = locale;
            m_hasLocale.store(locale != "C" && locale != "POSIX" && !locale.empty(), std::memory_order_release);
        }

        std::string getLocale() const {
            std::lock_guard<std::mutex> lock(m_localeMutex);
            return m_locale;
        }

        /// Set a per-sink locale. The sink's formatter will use this locale
        /// to re-render culture-specific format specifiers, overriding the
        /// logger-level locale for that sink only.
        void setSinkLocale(size_t sinkIndex, const std::string& locale) {
            m_logManager.setSinkLocale(sinkIndex, locale);
        }

        /// Register an enricher that attaches metadata to every log entry.
        /// Enrichers run in registration order before explicit context
        /// (setContext / scoped context), so explicit context wins on
        /// key collisions.
        ///
        /// @throws std::logic_error if called after the first log entry
        ///         has been emitted. All enrichers must be registered
        ///         during logger configuration, before any logging begins.
        void enrich(EnricherFn fn) {
            if (m_logManager.isLoggingStarted()) {
                throw std::logic_error("Cannot add enrichers after logging has started");
            }
            m_enrichers.push_back(std::move(fn));
            m_hasEnrichers.store(true, std::memory_order_release);
        }

    private:
        friend class LoggerConfiguration;

        struct BuilderTag {};
        explicit LunarLog(BuilderTag)
            : m_minLevel(LogLevel::TRACE)
            , m_isRunning(true)
            , m_rateLimitWindowStart(std::chrono::steady_clock::now().time_since_epoch().count())
            , m_logCount(0)
            , m_captureSourceLocation(false)
            , m_hasCustomContext(false)
            , m_sinkWriteInProgress(false)
            , m_templateCacheSize(128)
            , m_hasGlobalFilters(false)
            , m_rateLimitMaxLogs(1000)
            , m_rateLimitWindowMs(1000)
            , m_threadStarted(false) {
        }

        void ensureProcessingThread() {
            if (!m_threadStarted.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (!m_threadStarted.load(std::memory_order_relaxed)) {
                    m_logThread = std::thread(&LunarLog::processLogQueue, this);
                    m_threadStarted.store(true, std::memory_order_release);
                }
            }
        }

        std::atomic<LogLevel> m_minLevel;
        std::atomic<bool> m_isRunning;
        std::atomic<long long> m_rateLimitWindowStart;
        std::atomic<size_t> m_logCount;
        std::mutex m_queueMutex;
        std::mutex m_contextMutex;
        std::condition_variable m_logCV;
        std::condition_variable m_flushCV;
        std::queue<LogEntry> m_logQueue;
        std::thread m_logThread;
        LogManager m_logManager;
        std::map<std::string, std::string> m_customContext;
        std::atomic<bool> m_captureSourceLocation;
        std::atomic<bool> m_hasCustomContext;
        std::atomic<bool> m_sinkWriteInProgress;

        struct PlaceholderInfo {
            std::string name;
            std::string fullContent;
            std::string spec;
            size_t startPos;
            size_t endPos;
            char operator_;  // '@' (destructure), '$' (stringify), or 0 (none)
            std::vector<detail::Transform> transforms;
            int indexedArg;  // >= 0 for indexed ({0},{1},...), -1 for named
            int alignment;   // >0 right-align, <0 left-align, 0 = none
        };

        std::mutex m_cacheMutex;
        std::unordered_map<std::string, std::vector<PlaceholderInfo>> m_templateCache;
        size_t m_templateCacheSize;

        std::mutex m_globalFilterMutex;
        std::atomic<bool> m_hasGlobalFilters;
        FilterPredicate m_globalFilter;
        std::vector<FilterRule> m_globalFilterRules;

        mutable std::mutex m_localeMutex;
        std::atomic<bool> m_hasLocale{false};
        std::string m_locale = "C";

        std::vector<EnricherFn> m_enrichers;
        std::atomic<bool> m_hasEnrichers{false};

        size_t m_rateLimitMaxLogs;
        long long m_rateLimitWindowMs;
        std::atomic<bool> m_threadStarted;

        static std::vector<PlaceholderInfo> extractPlaceholders(const std::string &messageTemplate) {
            std::vector<PlaceholderInfo> placeholders;
            detail::forEachPlaceholder(messageTemplate, [&](const detail::ParsedPlaceholder& ph) {
                placeholders.push_back({ph.name, ph.fullContent, ph.spec, ph.startPos, ph.endPos, ph.op, ph.transforms, ph.indexedArg, ph.alignment});
            });
            return placeholders;
        }

        template<typename... Args>
        void logInternal(LogLevel level, const char* file, int line, const char* function, const std::string &messageTemplate, const Args &... args) {
            if (!m_isRunning.load(std::memory_order_acquire)) return;
            if (level < m_minLevel.load(std::memory_order_relaxed)) return;
            if (!rateLimitCheck()) return;

            std::vector<std::string> values{toString(args)...};
            detail::ExceptionInfo noException;
            emitLogEntry(level, file, line, function, messageTemplate, values, noException);
        }

        template<typename... Args>
        void logInternalWithException(LogLevel level, const char* file, int line, const char* function,
                                      const std::exception& ex, const std::string &messageTemplate, const Args &... args) {
            if (!m_isRunning.load(std::memory_order_acquire)) return;
            if (level < m_minLevel.load(std::memory_order_relaxed)) return;
            if (!rateLimitCheck()) return;

            std::vector<std::string> values{toString(args)...};
            detail::ExceptionInfo exInfo = detail::extractExceptionInfo(ex);
            emitLogEntry(level, file, line, function, messageTemplate, values, exInfo);
        }

        void emitLogEntry(LogLevel level, const char* file, int line, const char* function,
                          const std::string &messageTemplate, std::vector<std::string>& values,
                          detail::ExceptionInfo& exInfo) {
            auto now = std::chrono::system_clock::now();

            auto tagResult = detail::parseTags(messageTemplate);
            std::vector<std::string> entryTags = std::move(tagResult.first);
            const std::string& effectiveTemplate = entryTags.empty() ? messageTemplate : tagResult.second;

            uint32_t hash = detail::fnv1a(effectiveTemplate);
            std::vector<PlaceholderInfo> placeholders;
            bool cacheHit = false;
            {
                std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
                if (m_templateCacheSize > 0) {
                    auto it = m_templateCache.find(effectiveTemplate);
                    if (it != m_templateCache.end()) {
                        placeholders = it->second;
                        cacheHit = true;
                    }
                }
            }
            if (!cacheHit) {
                placeholders = extractPlaceholders(effectiveTemplate);
                std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
                if (m_templateCacheSize > 0) {
                    if (m_templateCache.size() < m_templateCacheSize) {
                        m_templateCache[effectiveTemplate] = placeholders;
                    }
                }
            }

            std::string localeCopy = "C";
            if (m_hasLocale.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> localeLock(m_localeMutex);
                localeCopy = m_locale;
            }

            std::vector<std::string> warnings = validatePlaceholders(effectiveTemplate, placeholders, values);
            std::string message = formatMessage(effectiveTemplate, placeholders, values, localeCopy);
            auto argumentPairs = mapArgumentsToPlaceholders(placeholders, values);
            auto properties = mapProperties(placeholders, values);

            bool captureCtx = m_captureSourceLocation.load(std::memory_order_relaxed);
            std::map<std::string, std::string> contextCopy;
            if (m_hasCustomContext.load(std::memory_order_acquire)) {
                std::lock_guard<std::mutex> contextLock(m_contextMutex);
                contextCopy = m_customContext;
            }

            auto& scopeStack = detail::threadScopeStack();
            if (!scopeStack.empty()) {
                for (const auto& frame : scopeStack) {
                    for (const auto& kv : frame) {
                        contextCopy[kv.first] = kv.second;
                    }
                }
            }

            bool hasEnrichers = m_hasEnrichers.load(std::memory_order_acquire);

            LogEntry entry(
                level, std::move(message), now, effectiveTemplate, hash,
                std::move(argumentPairs),
                captureCtx ? file : "", captureCtx ? line : 0, captureCtx ? function : "",
                std::map<std::string, std::string>(),
                std::move(properties), std::move(entryTags), std::move(localeCopy),
                std::this_thread::get_id(),
                std::move(exInfo.type), std::move(exInfo.message), std::move(exInfo.chain)
            );

            if (hasEnrichers) {
                for (const auto& enricher : m_enrichers) {
                    // Enricher exceptions are swallowed to prevent logging
                    // from crashing the application.
                    try {
                        enricher(entry);
                    } catch (...) {}
                }
                // Explicit context (setContext / scoped) overwrites enricher
                // values — key-by-key merge required for correct precedence.
                for (const auto& kv : contextCopy) {
                    entry.customContext[kv.first] = kv.second;
                }
            } else {
                // Fast path: no enrichers registered — move the entire context
                // map into the entry in O(1) instead of copying key-by-key.
                entry.customContext = std::move(contextCopy);
            }

            ensureProcessingThread();

            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_logQueue.push(std::move(entry));

            for (const auto& warning : warnings) {
                uint32_t warnHash = detail::fnv1a(warning);
                m_logQueue.emplace(LogEntry(
                    /* level */        LogLevel::WARN,
                    /* message */      warning,
                    /* timestamp */    now,
                    /* templateStr */  warning,
                    /* templateHash */ warnHash,
                    /* arguments */    {},
                    /* file */         captureCtx ? file : "",
                    /* line */         captureCtx ? line : 0,
                    /* function */     captureCtx ? function : "",
                    /* customContext */{},
                    /* properties */   {},
                    /* tags */         {},
                    /* locale */       "C",
                    /* threadId */     std::this_thread::get_id()
                ));
            }

            lock.unlock();
            m_logCV.notify_one();
        }

        void processLogQueue() {
            while (m_isRunning) {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_logCV.wait(lock, [this] { return !m_logQueue.empty() || !m_isRunning; });

                while (!m_logQueue.empty()) {
                    auto entry = std::move(m_logQueue.front());
                    m_logQueue.pop();
                    m_sinkWriteInProgress.store(true, std::memory_order_relaxed);
                    lock.unlock();

                    try {
                        m_logManager.log(entry, m_globalFilter, m_globalFilterRules, m_globalFilterMutex, m_hasGlobalFilters);
                    } catch (...) {
                        // Swallow — logging must not crash the application
                    }

                    // Re-acquire lock BEFORE clearing sinkWriteInProgress and
                    // notifying, so that flush() cannot miss the state change.
                    // Without this, flush() can check the predicate (seeing
                    // sinkWriteInProgress==true), then we set it to false and
                    // notify, and THEN flush() enters wait — a classic lost wakeup.
                    lock.lock();
                    m_sinkWriteInProgress.store(false, std::memory_order_relaxed);
                    m_flushCV.notify_all();
                }
            }

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                while (!m_logQueue.empty()) {
                    auto entry = std::move(m_logQueue.front());
                    m_logQueue.pop();
                    m_sinkWriteInProgress.store(true, std::memory_order_relaxed);
                    lock.unlock();
                    try {
                        m_logManager.log(entry, m_globalFilter, m_globalFilterRules, m_globalFilterMutex, m_hasGlobalFilters);
                    } catch (...) {
                        // Swallow — logging must not crash the application
                    }
                    lock.lock();
                    m_sinkWriteInProgress.store(false, std::memory_order_relaxed);
                    m_flushCV.notify_all();
                }
            }
        }

        // Rate limiting is best-effort under concurrent access.
        //
        // When the window expires, the first thread to win the CAS resets
        // m_rateLimitWindowStart and stores m_logCount to 1 (counting its own
        // message). Concurrent threads that already read the old window but
        // lose the CAS retry and see the new window; their subsequent
        // fetch_add on m_logCount races with the store(1), so a small number
        // of messages at the window boundary may be silently lost or allowed
        // beyond the limit. This is acceptable for a best-effort rate limiter.
        bool rateLimitCheck() {
            auto now = std::chrono::steady_clock::now();
            long long nowNs = now.time_since_epoch().count();
            long long windowStart = m_rateLimitWindowStart.load(std::memory_order_relaxed);

            for (;;) {
                auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::duration(nowNs - windowStart)).count();
                if (durationMs < m_rateLimitWindowMs) break;
                if (m_rateLimitWindowStart.compare_exchange_weak(windowStart, nowNs,
                        std::memory_order_relaxed, std::memory_order_relaxed)) {
                    // Release so that the new count is visible to threads that
                    // subsequently read it with acquire in fetch_add below.
                    m_logCount.store(1, std::memory_order_release);
                    return true;
                }
            }
            // Acquire pairs with the release-store above: if a window reset just
            // happened, this thread sees the updated count before incrementing.
            size_t count = m_logCount.fetch_add(1, std::memory_order_acquire);
            if (count >= m_rateLimitMaxLogs) {
                return false;
            }
            return true;
        }

        static bool isWhitespaceOnly(const std::string &s) {
            for (size_t i = 0; i < s.size(); ++i) {
                if (!std::isspace(static_cast<unsigned char>(s[i]))) return false;
            }
            return !s.empty();
        }

        static std::vector<std::string> validatePlaceholders(
            const std::string &templateStr,
            const std::vector<PlaceholderInfo> &placeholders,
            const std::vector<std::string> &values) {
            std::vector<std::string> warnings;
            std::set<std::string> uniquePlaceholders;

            for (const auto &ph : placeholders) {
                if (ph.name.empty()) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has empty placeholder");
                } else if (isWhitespaceOnly(ph.name)) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has whitespace-only placeholder name");
                } else if (ph.indexedArg < 0 && !uniquePlaceholders.insert(ph.name).second) {
                    warnings.push_back("Warning: Template \"" + templateStr + "\" has duplicate placeholder name: " + ph.name);
                }
            }

            std::set<size_t> usedSlots;
            size_t namedOrdinal = 0;
            for (size_t i = 0; i < placeholders.size(); ++i) {
                const auto &ph = placeholders[i];
                size_t slot = detail::resolveValueSlot(ph.indexedArg, namedOrdinal);
                if (ph.indexedArg < 0) ++namedOrdinal;
                usedSlots.insert(slot);
            }

            if (usedSlots.size() < values.size()) {
                warnings.push_back("Warning: More values provided than placeholders");
            }

            bool hasMissingSlot = false;
            for (std::set<size_t>::const_iterator it = usedSlots.begin(); it != usedSlots.end(); ++it) {
                if (*it >= values.size()) {
                    hasMissingSlot = true;
                    break;
                }
            }
            if (hasMissingSlot) {
                warnings.push_back("Warning: More placeholders than provided values");
            }

            return warnings;
        }

        template<typename T>
        static std::string toString(const T &value) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }

        static std::string toString(const std::string &value) {
            return value;
        }

        static std::string toString(const char *value) {
            if (!value) return "(null)";
            return std::string(value);
        }

        static std::string toString(std::nullptr_t) {
            return "(null)";
        }

        static std::string toString(int value) {
            return std::to_string(value);
        }

        static std::string toString(long value) {
            return std::to_string(value);
        }

        static std::string toString(long long value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned int value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned long value) {
            return std::to_string(value);
        }

        static std::string toString(unsigned long long value) {
            return std::to_string(value);
        }

        static std::string toString(bool value) {
            return value ? "true" : "false";
        }

        static std::string toString(double value) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.15g", value);
            // Locale-safety: some locales use ',' as decimal separator;
            // ensure consistent '.' for downstream parsers (e.g. JSON).
            for (char *p = buf; *p; ++p) { if (*p == ',') *p = '.'; }
            return std::string(buf);
        }

        static std::string toString(float value) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(value));
            // Locale-safety: same as toString(double).
            for (char *p = buf; *p; ++p) { if (*p == ',') *p = '.'; }
            return std::string(buf);
        }

        // ----------------------------------------------------------------
        // Format helpers — delegate to detail:: namespace implementations.
        // Kept as private statics for internal use within LunarLog.
        // ----------------------------------------------------------------

        static int safeStoi(const std::string &s, int fallback = 0) {
            return detail::safeStoi(s, fallback);
        }

        static bool tryParseDouble(const std::string &s, double &out) {
            return detail::tryParseDouble(s, out);
        }

        static bool isNumericString(const std::string &s) {
            double ignored;
            return detail::tryParseDouble(s, ignored);
        }

        static double parseDouble(const std::string &s) {
            double val = 0.0;
            detail::tryParseDouble(s, val);
            return val;
        }

        /// Apply a format spec to a value, with locale for culture-specific specs.
        /// Delegates to detail::applyFormat which handles all specs including
        /// culture-specific ones (:n, :N, :d, :D, :t, :T, :f, :F).
        static std::string applyFormat(const std::string &value, const std::string &spec, const std::string &locale = "C") {
            return detail::applyFormat(value, spec, locale);
        }

        static std::pair<std::string, std::string> splitPlaceholder(const std::string &placeholder) {
            return detail::splitPlaceholder(placeholder);
        }

        static std::string formatMessage(const std::string &messageTemplate,
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values,
            const std::string &locale = "C") {
            return detail::walkTemplate(messageTemplate, placeholders, values, locale);
        }

        static std::vector<std::pair<std::string, std::string>> mapArgumentsToPlaceholders(
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values) {
            std::vector<std::pair<std::string, std::string>> argumentPairs;

            size_t namedOrdinal = 0;
            for (size_t i = 0; i < placeholders.size(); ++i) {
                const auto &ph = placeholders[i];
                size_t valueIdx = detail::resolveValueSlot(ph.indexedArg, namedOrdinal);
                if (ph.indexedArg < 0) ++namedOrdinal;
                if (valueIdx < values.size()) {
                    argumentPairs.emplace_back(ph.name, values[valueIdx]);
                }
            }

            return argumentPairs;
        }

        /// Map placeholders to PlaceholderProperty entries for structured formatters.
        /// @param placeholders Parsed placeholder metadata (includes operator info).
        /// @param values       Positional argument strings (from toString() conversion).
        /// Properties is the richer representation: it carries operator context (@/$)
        /// that arguments (simple name-value pairs) do not, enabling formatters to
        /// choose type-aware serialization (e.g. JSON native types for @).
        /// @note Properties are populated unconditionally — even when no operators
        ///       are used — so that structured formatters (JSON, XML) always have
        ///       access to placeholder names.  The per-message overhead is a single
        ///       reserve + N moves for typical placeholder counts (< 10), which is
        ///       negligible relative to I/O.
        static std::vector<PlaceholderProperty> mapProperties(
            const std::vector<PlaceholderInfo> &placeholders, const std::vector<std::string> &values) {
            std::vector<PlaceholderProperty> props;
            props.reserve(placeholders.size());
            std::set<std::string> seen;

            size_t namedOrdinal = 0;
            for (size_t i = 0; i < placeholders.size(); ++i) {
                const auto &ph = placeholders[i];
                size_t valueIdx = detail::resolveValueSlot(ph.indexedArg, namedOrdinal);
                if (ph.indexedArg < 0) ++namedOrdinal;
                if (valueIdx >= values.size()) continue;
                if (!seen.insert(ph.name).second) continue;
                char effectiveOp = ph.operator_;
                std::vector<std::string> transformSpecs;
                for (size_t ti = 0; ti < ph.transforms.size(); ++ti) {
                    const auto &t = ph.transforms[ti];
                    if (t.name == "expand") effectiveOp = '@';
                    else if (t.name == "str") effectiveOp = '$';
                    if (t.arg.empty()) {
                        transformSpecs.push_back(t.name);
                    } else {
                        transformSpecs.push_back(t.name + ":" + t.arg);
                    }
                }
                props.push_back({ph.name, values[valueIdx], effectiveOp, std::move(transformSpecs)});
            }

            return props;
        }
    };

    /// RAII scoped context that injects key-value pairs into log entries
    /// for the lifetime of the scope.
    ///
    /// NOTE: Scoped context is **thread-wide**, not per-logger instance.
    /// All LunarLog instances on the same thread share the same scope stack.
    /// If you use multiple loggers on one thread, scoped context will appear
    /// in log entries from all of them. This is by design — most applications
    /// use a single logger, and thread-wide scoping avoids the complexity of
    /// passing logger pointers into scope objects.
    class LogScope {
    public:
        LogScope(const LogScope&) = delete;
        LogScope& operator=(const LogScope&) = delete;

        LogScope(LogScope&& other) noexcept
            : m_active(other.m_active), m_iter(other.m_iter) {
            other.m_active = false;
        }

        LogScope& operator=(LogScope&& other) noexcept {
            if (this != &other) {
                if (m_active) {
                    detail::threadScopeStack().erase(m_iter);
                }
                m_active = other.m_active;
                m_iter = other.m_iter;
                other.m_active = false;
            }
            return *this;
        }

        ~LogScope() noexcept {
            if (m_active) {
                detail::threadScopeStack().erase(m_iter);
            }
        }

        /// Append a key-value pair to this scope's frame.
        /// If the same key is added multiple times, the last value wins
        /// (later entries overwrite earlier ones during collection).
        LogScope& add(const std::string& key, const std::string& value) {
            if (m_active) {
                m_iter->emplace_back(key, value);
            }
            return *this;
        }

    private:
        friend class LunarLog;

        explicit LogScope(std::initializer_list<std::pair<std::string, std::string>> pairs)
            : m_active(true) {
            auto& stack = detail::threadScopeStack();
            stack.emplace_back(pairs.begin(), pairs.end());
            m_iter = std::prev(stack.end());
        }

        bool m_active = false;
        detail::ScopeFrameIter m_iter;
    };

    inline LogScope LunarLog::scope(std::initializer_list<std::pair<std::string, std::string>> pairs) {
        return LogScope(pairs);
    }

    class ContextScope {
    public:
        ContextScope(LunarLog& logger, const std::string& key, const std::string& value)
            : m_logger(logger), m_key(key) {
            m_logger.setContext(key, value);
        }

        ~ContextScope() noexcept {
            m_logger.clearContext(m_key);
        }

        ContextScope(const ContextScope &) = delete;
        ContextScope &operator=(const ContextScope &) = delete;
        ContextScope(ContextScope &&) = delete;
        ContextScope &operator=(ContextScope &&) = delete;

    private:
        LunarLog& m_logger;
        std::string m_key;
    };

    inline SinkProxy LunarLog::sink(const std::string& name) {
        size_t idx = m_logManager.getSinkIndex(name);
        return SinkProxy(m_logManager.getSink(idx), m_logManager.isLoggingStarted());
    }

    inline LoggerConfiguration LunarLog::configure() {
        return LoggerConfiguration();
    }

    inline LunarLog LoggerConfiguration::build() {
        if (m_built) {
            throw std::logic_error(
                "LoggerConfiguration::build() already called");
        }
        m_built = true;

        // Create a bare LunarLog — no default console sink, no thread.
        LunarLog logger(LunarLog::BuilderTag{});

        // Apply global settings via friend access.
        logger.m_minLevel.store(m_minLevel, std::memory_order_relaxed);
        logger.m_captureSourceLocation.store(
            m_captureSourceLocation, std::memory_order_relaxed);
        logger.m_rateLimitMaxLogs  = m_rateLimitMaxLogs;
        logger.m_rateLimitWindowMs = m_rateLimitWindowMs;
        logger.setTemplateCacheSize(m_templateCacheSize);

        if (!m_locale.empty()) {
            logger.setLocale(m_locale);
        }

        // Register enrichers (must happen before first log entry).
        for (size_t i = 0; i < m_enrichers.size(); ++i) {
            logger.enrich(std::move(m_enrichers[i]));
        }

        // Add compact filter expressions.
        for (size_t i = 0; i < m_filterCompact.size(); ++i) {
            logger.filter(m_filterCompact[i]);
        }

        // Add DSL filter rules.
        for (size_t i = 0; i < m_filterRules.size(); ++i) {
            logger.addFilterRule(m_filterRules[i]);
        }

        // Add sinks.
        if (m_sinks.empty()) {
            std::fprintf(stderr, "[LunarLog] Warning: build() called with no "
                                 "sinks — logger will silently discard all "
                                 "messages.\n");
        }
        for (size_t i = 0; i < m_sinks.size(); ++i) {
            if (m_sinks[i].hasName) {
                logger.addCustomSink(m_sinks[i].name,
                                     std::move(m_sinks[i].sink));
            } else {
                logger.addCustomSink(std::move(m_sinks[i].sink));
            }
        }

        // Start the background processing thread.
        logger.ensureProcessingThread();

        // Safe to return by move: LunarLog's move constructor transfers all
        // internal state (queue, sinks, atomics, enrichers, filters).  The
        // processing thread has not yet received any log entries at this point
        // because ensureProcessingThread() only starts the thread — it does
        // not produce entries.  The moved-from instance is left in a valid
        // but empty state (m_isRunning == false).
        return logger;
    }

} // namespace minta

#endif // LUNAR_LOG_SOURCE_HPP
