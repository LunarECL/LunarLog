#ifndef LUNAR_LOG_SYSLOG_SINK_HPP
#define LUNAR_LOG_SYSLOG_SINK_HPP

#ifndef _WIN32

#include "sink_interface.hpp"
#include "../core/log_common.hpp"
#include <syslog.h>
#include <string>
#include <atomic>
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

    public:
        /// @param ident  The syslog identity string (typically the program name).
        ///               Stored internally — the pointer passed to openlog() remains
        ///               valid for the lifetime of this sink.
        /// @param opts   Syslog configuration options.
        explicit SyslogSink(const std::string& ident,
                            SyslogOptions opts = SyslogOptions())
            : m_ident(ident)
            , m_opts(opts)
        {
            if (instanceRefCount().fetch_add(1, std::memory_order_acq_rel) > 0) {
                std::fprintf(stderr, "[LunarLog][SyslogSink] WARNING: multiple SyslogSink "
                                     "instances detected. openlog() is process-global; "
                                     "the last-created instance's ident will be used "
                                     "for all syslog output.\n");
            }
            openlog(m_ident.c_str(), m_opts.logopt_, m_opts.facility_);
        }

        ~SyslogSink() noexcept {
            if (instanceRefCount().fetch_sub(1, std::memory_order_acq_rel) == 1) {
                closelog();
            }
        }

        void write(const LogEntry& entry) override {
            int priority = toSyslogPriority(entry.level);

            if (m_opts.includeLevel_) {
                std::string msg = "[";
                msg += getLevelString(entry.level);
                msg += "] ";
                msg += entry.message;
                syslog(priority, "%s", msg.c_str());
            } else {
                syslog(priority, "%s", entry.message.c_str());
            }
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
        std::string m_ident;
        SyslogOptions m_opts;
    };

} // namespace minta

#endif // !_WIN32

#endif // LUNAR_LOG_SYSLOG_SINK_HPP
