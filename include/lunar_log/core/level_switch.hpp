#ifndef LUNAR_LOG_LEVEL_SWITCH_HPP
#define LUNAR_LOG_LEVEL_SWITCH_HPP

#include "log_level.hpp"
#include <atomic>
#include <memory>

namespace minta {

    /// Shared observable log level that can be changed at runtime from any thread.
    ///
    /// Multiple loggers can share a single LevelSwitch via shared_ptr.
    /// When the level is changed via set(), all loggers using this switch
    /// see the new level immediately (on the next log call).
    ///
    /// Usage:
    /// @code
    ///   auto levelSwitch = std::make_shared<LevelSwitch>(LogLevel::INFO);
    ///   auto logger = LunarLog::configure()
    ///       .minLevel(levelSwitch)
    ///       .writeTo<ConsoleSink>("console")
    ///       .build();
    ///
    ///   // Later, from any thread:
    ///   levelSwitch->set(LogLevel::DEBUG);  // takes effect immediately
    /// @endcode
    ///
    /// Thread safety: get() and set() are lock-free atomic operations.
    ///
    /// Memory ordering: both use memory_order_relaxed.  A set() on one
    /// thread becomes visible to get() on other threads without formal
    /// ordering guarantees, but propagates within nanoseconds on all
    /// mainstream architectures (x86 TSO, ARM DMB on atomics).  This is
    /// intentional: log-level changes are advisory, and occasional stale
    /// reads are acceptable (a few extra or missed log entries at the
    /// boundary).  This matches the ordering used for m_minLevel
    /// throughout LunarLog.
    class LevelSwitch {
    public:
        /// Construct a LevelSwitch with an initial log level.
        explicit LevelSwitch(LogLevel initial = LogLevel::INFO)
            : m_level(initial) {}

        /// Get the current log level.
        /// Lock-free, safe to call from any thread.
        LogLevel get() const {
            return m_level.load(std::memory_order_relaxed);
        }

        /// Set the log level. Takes effect immediately for all loggers
        /// sharing this LevelSwitch.
        /// Lock-free, safe to call from any thread.
        void set(LogLevel level) {
            m_level.store(level, std::memory_order_relaxed);
        }

    private:
        std::atomic<LogLevel> m_level;
    };

} // namespace minta

#endif // LUNAR_LOG_LEVEL_SWITCH_HPP
