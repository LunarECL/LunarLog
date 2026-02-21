#ifndef LUNAR_LOG_GLOBAL_HPP
#define LUNAR_LOG_GLOBAL_HPP

#include "log_source.hpp"
#include "logger_configuration.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace minta {

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
    /// Thread safety: init/shutdown acquire a mutex. Logging methods acquire
    /// the mutex to read the pointer (shared_ptr-style fast path via atomic
    /// load would be possible but C++11 constrains us to mutex).
    class Log {
    public:
        Log() = delete;

        /// Create a LoggerConfiguration builder.  When build() is called on
        /// the returned builder, the global instance is set automatically.
        static LoggerConfiguration configure() {
            return LoggerConfiguration();
        }

        /// Set the global logger from a pre-built LunarLog instance.
        /// Replaces any existing global logger (previous instance is destroyed).
        static void init(LunarLog logger) {
            std::lock_guard<std::mutex> lock(mutex());
            storage() = detail::make_unique<LunarLog>(std::move(logger));
        }

        /// Destroy the global logger instance.
        /// After shutdown, all logging methods throw std::logic_error.
        static void shutdown() {
            std::lock_guard<std::mutex> lock(mutex());
            storage().reset();
        }

        /// Returns true if a global logger has been configured and not shut down.
        static bool isInitialized() {
            std::lock_guard<std::mutex> lock(mutex());
            return storage() != nullptr;
        }

        /// Access the global LunarLog instance.
        /// @throws std::logic_error if not initialized.
        static LunarLog& instance() {
            std::lock_guard<std::mutex> lock(mutex());
            auto& ptr = storage();
            if (!ptr) {
                throw std::logic_error(
                    "minta::Log not initialized. Call Log::init() or "
                    "Log::configure().build() first.");
            }
            return *ptr;
        }

        // --- Logging methods ---
        // Each acquires the mutex, checks initialization, and delegates.

        template<typename... Args>
        static void trace(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::TRACE, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::DEBUG, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::INFO, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::WARN, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void error(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::ERROR, msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void fatal(const std::string& msg, Args&&... args) {
            std::lock_guard<std::mutex> lock(mutex());
            requireInit().log(LogLevel::FATAL, msg, std::forward<Args>(args)...);
        }

    private:
        static std::unique_ptr<LunarLog>& storage() {
            static std::unique_ptr<LunarLog> s_logger;
            return s_logger;
        }

        static std::mutex& mutex() {
            static std::mutex s_mutex;
            return s_mutex;
        }

        static LunarLog& requireInit() {
            auto& ptr = storage();
            if (!ptr) {
                throw std::logic_error(
                    "minta::Log not initialized. Call Log::init() or "
                    "Log::configure().build() first.");
            }
            return *ptr;
        }
    };

} // namespace minta

// --- Convenience macros ---
#ifndef LUNAR_LOG_NO_GLOBAL_MACROS

#define LUNAR_GTRACE(msg, ...) ::minta::Log::trace(msg, ##__VA_ARGS__)
#define LUNAR_GDEBUG(msg, ...) ::minta::Log::debug(msg, ##__VA_ARGS__)
#define LUNAR_GINFO(msg, ...)  ::minta::Log::info(msg, ##__VA_ARGS__)
#define LUNAR_GWARN(msg, ...)  ::minta::Log::warn(msg, ##__VA_ARGS__)
#define LUNAR_GERROR(msg, ...) ::minta::Log::error(msg, ##__VA_ARGS__)
#define LUNAR_GFATAL(msg, ...) ::minta::Log::fatal(msg, ##__VA_ARGS__)

#endif // LUNAR_LOG_NO_GLOBAL_MACROS

#endif // LUNAR_LOG_GLOBAL_HPP
