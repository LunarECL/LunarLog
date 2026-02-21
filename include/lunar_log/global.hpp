#ifndef LUNAR_LOG_GLOBAL_HPP
#define LUNAR_LOG_GLOBAL_HPP

#include "log_source.hpp"
#include "logger_configuration.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace minta {

    // Forward declaration — defined after Log class.
    class GlobalLoggerConfiguration;

    /// Static global logger facade.
    ///
    /// Provides a process-wide singleton LunarLog instance accessible from
    /// anywhere without passing logger references.
    ///
    /// Usage:
    /// @code
    ///   minta::Log::configure()
    ///       .minLevel(minta::LogLevel::DEBUG)
    ///       .writeTo<minta::ConsoleSink>()
    ///       .build();   // sets the global instance
    ///
    ///   minta::Log::info("Hello {name}", "name", "world");
    ///   minta::Log::shutdown();
    /// @endcode
    ///
    /// Thread safety: all methods acquire a mutex only to copy a shared_ptr
    /// to the underlying LunarLog instance.  The actual log() call runs
    /// outside the mutex, so concurrent producers are not serialized.
    /// init()/shutdown() are safe to call from any thread; in-flight log
    /// calls that already obtained a shared_ptr will complete safely even
    /// if shutdown() runs concurrently.
    class Log {
    public:
        Log() = delete;

        /// Create a builder whose build() automatically sets the global
        /// logger instance.  Returns a GlobalLoggerConfiguration that
        /// forwards all LoggerConfiguration methods.
        static GlobalLoggerConfiguration configure();

        /// Set the global logger from a pre-built LunarLog instance.
        /// Replaces any existing global logger (previous instance is
        /// destroyed when all in-flight references are released).
        static void init(LunarLog logger) {
            auto ptr = std::make_shared<LunarLog>(std::move(logger));
            {
                std::lock_guard<std::mutex> lock(mutex());
                storage().swap(ptr); // ptr now holds the old logger (if any)
            }
            // ptr (old logger) destructs here, outside the lock, to
            // prevent deadlocks if its destructor drains queued entries.
        }

        /// Clear the global logger instance.
        /// After shutdown, all logging methods throw std::logic_error.
        /// The underlying LunarLog is destroyed outside the lock to
        /// prevent deadlocks if its destructor drains queued entries.
        static void shutdown() {
            std::shared_ptr<LunarLog> old;
            {
                std::lock_guard<std::mutex> lock(mutex());
                old = std::move(storage());
            }
            // old destructs here, outside the lock.
        }

        /// Returns true if a global logger has been configured and not shut down.
        static bool isInitialized() {
            std::lock_guard<std::mutex> lock(mutex());
            return storage() != nullptr;
        }

        /// Access the global LunarLog instance via shared_ptr.
        /// The returned shared_ptr keeps the logger alive even if
        /// shutdown() is called concurrently; the caller can safely
        /// use the logger through the returned handle.
        /// @throws std::logic_error if not initialized.
        static std::shared_ptr<LunarLog> instance() {
            std::lock_guard<std::mutex> lock(mutex());
            auto ptr = storage();
            if (!ptr) {
                throw std::logic_error(
                    "minta::Log not initialized. Call Log::init() or "
                    "Log::configure().build() first.");
            }
            return ptr;
        }

        /// Flush all sinks on the global logger.
        /// Convenience method so callers do not need instance().
        /// @throws std::logic_error if not initialized.
        static void flush() {
            requireInit()->flush();
        }

        // --- Logging methods ---
        // Each copies the shared_ptr under the lock, then calls log()
        // outside the lock so concurrent producers are not serialized.

        template<typename... Args>
        static void trace(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::TRACE, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::DEBUG, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::INFO, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::WARN, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::ERROR, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void fatal(const std::string& msg, Args&&... args) {
            requireInit()->log(LogLevel::FATAL, msg, std::forward<Args>(args)...);
        }

    private:
        static std::shared_ptr<LunarLog>& storage() {
            static std::shared_ptr<LunarLog> s_logger;
            return s_logger;
        }

        static std::mutex& mutex() {
            static std::mutex s_mutex;
            return s_mutex;
        }

        /// Lock, copy shared_ptr, unlock, check non-null.
        /// Returns a live shared_ptr — the logger stays alive for the
        /// duration of the caller's use even if shutdown() intervenes.
        static std::shared_ptr<LunarLog> requireInit() {
            std::shared_ptr<LunarLog> ptr;
            {
                std::lock_guard<std::mutex> lock(mutex());
                ptr = storage();
            }
            if (!ptr) {
                throw std::logic_error(
                    "minta::Log not initialized. Call Log::init() or "
                    "Log::configure().build() first.");
            }
            return ptr;
        }
    };

    // -----------------------------------------------------------------
    //  GlobalLoggerConfiguration
    // -----------------------------------------------------------------

    /// Builder wrapper returned by Log::configure().
    /// Forwards all LoggerConfiguration methods and automatically calls
    /// Log::init() when build() is invoked.
    class GlobalLoggerConfiguration {
    public:
        GlobalLoggerConfiguration() = default;
        GlobalLoggerConfiguration(const GlobalLoggerConfiguration&) = delete;
        GlobalLoggerConfiguration& operator=(const GlobalLoggerConfiguration&) = delete;
        GlobalLoggerConfiguration(GlobalLoggerConfiguration&&) = default;
        GlobalLoggerConfiguration& operator=(GlobalLoggerConfiguration&&) = default;

        // --- Forwarded builder methods ---

        GlobalLoggerConfiguration& minLevel(LogLevel level) {
            m_config.minLevel(level); return *this;
        }
        GlobalLoggerConfiguration& captureSourceLocation(bool enable) {
            m_config.captureSourceLocation(enable); return *this;
        }
        GlobalLoggerConfiguration& rateLimit(size_t maxLogs,
                                             std::chrono::milliseconds window) {
            m_config.rateLimit(maxLogs, window); return *this;
        }
        GlobalLoggerConfiguration& templateCacheSize(size_t size) {
            m_config.templateCacheSize(size); return *this;
        }
        GlobalLoggerConfiguration& locale(const std::string& loc) {
            m_config.locale(loc); return *this;
        }
        GlobalLoggerConfiguration& enrich(EnricherFn fn) {
            m_config.enrich(std::move(fn)); return *this;
        }
        GlobalLoggerConfiguration& filter(const std::string& compact) {
            m_config.filter(compact); return *this;
        }
        GlobalLoggerConfiguration& filterRule(const std::string& dsl) {
            m_config.filterRule(dsl); return *this;
        }

        // --- writeTo: unnamed sink, no formatter ---
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            m_config.writeTo<SinkType>(std::forward<Args>(args)...);
            return *this;
        }

        // --- writeTo: unnamed sink, with formatter ---
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(Args&&... args) {
            m_config.writeTo<SinkType, FormatterType>(
                std::forward<Args>(args)...);
            return *this;
        }

        // --- writeTo: named sink, no formatter ---
        template<typename SinkType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            m_config.writeTo<SinkType>(name, std::forward<Args>(args)...);
            return *this;
        }

        // --- writeTo: named sink, with formatter ---
        template<typename SinkType, typename FormatterType, typename... Args>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            std::is_constructible<SinkType, Args...>::value &&
            !std::is_constructible<SinkType, const std::string&, Args...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(const std::string& name, Args&&... args) {
            m_config.writeTo<SinkType, FormatterType>(
                name, std::forward<Args>(args)...);
            return *this;
        }

        // --- writeTo: named sink + config lambda ---
        template<typename SinkType, typename ConfigFn, typename... CtorArgs>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            !std::is_convertible<
                typename std::decay<ConfigFn>::type, std::string>::value &&
            std::is_constructible<SinkType, CtorArgs...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(const std::string& name, ConfigFn&& configure,
                CtorArgs&&... args) {
            m_config.writeTo<SinkType>(
                name, std::forward<ConfigFn>(configure),
                std::forward<CtorArgs>(args)...);
            return *this;
        }

        // --- writeTo: named sink + formatter + config lambda ---
        template<typename SinkType, typename FormatterType,
                 typename ConfigFn, typename... CtorArgs>
        typename std::enable_if<
            std::is_base_of<ISink, SinkType>::value &&
            std::is_base_of<IFormatter, FormatterType>::value &&
            !std::is_convertible<
                typename std::decay<ConfigFn>::type, std::string>::value &&
            std::is_constructible<SinkType, CtorArgs...>::value,
            GlobalLoggerConfiguration&
        >::type
        writeTo(const std::string& name, ConfigFn&& configure,
                CtorArgs&&... args) {
            m_config.writeTo<SinkType, FormatterType>(
                name, std::forward<ConfigFn>(configure),
                std::forward<CtorArgs>(args)...);
            return *this;
        }

        /// Build the LunarLog instance and set it as the global logger.
        void build() {
            Log::init(m_config.build());
        }

    private:
        LoggerConfiguration m_config;
    };

    // --- Out-of-class definition (needs complete GlobalLoggerConfiguration) ---

    inline GlobalLoggerConfiguration Log::configure() {
        return GlobalLoggerConfiguration();
    }

} // namespace minta

// --- Convenience macros ---
// Use standard __VA_ARGS__ (message is part of the variadic pack).
#ifndef LUNAR_LOG_NO_GLOBAL_MACROS

#define LUNAR_GTRACE(...) ::minta::Log::trace(__VA_ARGS__)
#define LUNAR_GDEBUG(...) ::minta::Log::debug(__VA_ARGS__)
#define LUNAR_GINFO(...)  ::minta::Log::info(__VA_ARGS__)
#define LUNAR_GWARN(...)  ::minta::Log::warn(__VA_ARGS__)
#define LUNAR_GERROR(...) ::minta::Log::error(__VA_ARGS__)
#define LUNAR_GFATAL(...) ::minta::Log::fatal(__VA_ARGS__)

#endif // LUNAR_LOG_NO_GLOBAL_MACROS

#endif // LUNAR_LOG_GLOBAL_HPP
