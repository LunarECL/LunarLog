#ifndef LUNAR_LOG_SYSLOG_SINK_HPP
#define LUNAR_LOG_SYSLOG_SINK_HPP

#ifndef _WIN32

#include "sink_interface.hpp"
#include "../core/log_common.hpp"
#include <syslog.h>
#include <string>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdio>

namespace minta {

    /// Configuration options for SyslogSink.
    struct SyslogOptions {
        int facility_;      ///< syslog facility (LOG_USER, LOG_LOCAL0, etc.)
        int logopt_;        ///< openlog() options (LOG_PID, LOG_NDELAY, etc.)
        bool includeLevel_; ///< Prefix messages with "[LEVEL] "

        SyslogOptions()
            : facility_(LOG_USER)
            , logopt_(LOG_PID | LOG_NDELAY)
            , includeLevel_(false) {}

        SyslogOptions& setFacility(int f) { facility_ = f; return *this; }
        SyslogOptions& setLogopt(int o) { logopt_ = o; return *this; }
        SyslogOptions& setIncludeLevel(bool b) { includeLevel_ = b; return *this; }
    };

    /// Sink that writes log entries to the POSIX syslog daemon.
    ///
    /// Uses the standard `<syslog.h>` API. No external dependencies.
    /// Available on Linux, macOS, and BSD. Not available on Windows —
    /// on Windows this header is silently skipped (no definitions are emitted).
    ///
    /// @note openlog() is a process-global call. Only one SyslogSink per process
    ///       is recommended. Multiple instances will overwrite each other's ident.
    ///
    /// @code
    ///   logger.addSink<SyslogSink>("my-app");
    ///   logger.addSink<SyslogSink>("my-app", SyslogOptions().setFacility(LOG_LOCAL0));
    /// @endcode
    class SyslogSink : public ISink {
        static std::atomic<int>& instanceRefCount() {
            static std::atomic<int> count(0);
            return count;
        }

        /// Process-global ident buffer.  Most syslog implementations (glibc,
        /// BSD libc) store the pointer passed to openlog() *without copying*
        /// the string.  A per-instance std::string would create a dangling
        /// pointer when instances are destroyed in non-LIFO order.  This
        /// fixed buffer ensures the pointer remains valid for the lifetime
        /// of the process.
        static const size_t kMaxIdentLen = 255;
        static char* globalIdent() {
            static char buf[kMaxIdentLen + 1] = {0};
            return buf;
        }
        static std::mutex& identMutex() {
            static std::mutex m;
            return m;
        }

    public:
        /// @param ident  The syslog identity string (typically the program name).
        ///               Copied into a process-global buffer.  The pointer passed
        ///               to openlog() points to this static buffer, ensuring it
        ///               remains valid regardless of instance destruction order.
        /// @param opts   Syslog configuration options.
        explicit SyslogSink(const std::string& ident,
                            SyslogOptions opts = SyslogOptions())
            : m_opts(opts)
        {
            // The refcount check and openlog() must be under the same mutex
            // to prevent a concurrent destructor from calling closelog()
            // between our fetch_add and openlog(), which would leave this
            // instance with a closed syslog connection.
            std::lock_guard<std::mutex> lock(identMutex());
            if (instanceRefCount().fetch_add(1, std::memory_order_relaxed) > 0) {
                std::fprintf(stderr, "[LunarLog][SyslogSink] WARNING: multiple SyslogSink "
                                     "instances detected. openlog() is process-global; "
                                     "the last-created instance's ident will be used "
                                     "for all syslog output.\n");
            }
            if (ident.size() >= kMaxIdentLen) {
                std::fprintf(stderr, "[LunarLog][SyslogSink] WARNING: ident \"%s\" "
                                     "truncated to %zu characters\n",
                             ident.c_str(), kMaxIdentLen - 1);
            }
            std::strncpy(globalIdent(), ident.c_str(), kMaxIdentLen);
            globalIdent()[kMaxIdentLen] = '\0';
            openlog(globalIdent(), m_opts.logopt_, m_opts.facility_);
        }

        ~SyslogSink() noexcept {
            // Must be under identMutex to serialize against concurrent
            // constructors — prevents closelog() after a new openlog().
            std::lock_guard<std::mutex> lock(identMutex());
            if (instanceRefCount().fetch_sub(1, std::memory_order_relaxed) == 1) {
                closelog();
            }
        }

        /// @note syslog() dereferences the ident pointer passed to openlog()
        /// on every call without copying the underlying buffer.  Serializing
        /// against identMutex() prevents data races when a concurrent
        /// constructor overwrites globalIdent() via strncpy, or a concurrent
        /// destructor tears down the connection via closelog().
        void write(const LogEntry& entry) override {
            int priority = toSyslogPriority(entry.level);

            // Build message outside the lock to minimize contention.
            const std::string* msgPtr = &entry.message;
            std::string prefixed;
            if (m_opts.includeLevel_) {
                prefixed.reserve(entry.message.size() + 10);
                prefixed += '[';
                prefixed += getLevelString(entry.level);
                prefixed += "] ";
                prefixed += entry.message;
                msgPtr = &prefixed;
            }

            std::lock_guard<std::mutex> lock(identMutex());
            syslog(priority, "%s", msgPtr->c_str());
        }

        /// Convert a LunarLog LogLevel to a syslog priority value.
        /// Public and static for testability.
        static int toSyslogPriority(LogLevel level) {
            switch (level) {
                case LogLevel::TRACE: return LOG_DEBUG;
                case LogLevel::DEBUG: return LOG_DEBUG;
                case LogLevel::INFO:  return LOG_INFO;
                case LogLevel::WARN:  return LOG_WARNING;
                case LogLevel::ERROR: return LOG_ERR;
                case LogLevel::FATAL: return LOG_CRIT;
                default: return LOG_INFO;
            }
        }

    private:
        SyslogOptions m_opts;
    };

} // namespace minta

#endif // !_WIN32

#endif // LUNAR_LOG_SYSLOG_SINK_HPP
