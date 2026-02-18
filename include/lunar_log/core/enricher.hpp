#ifndef LUNAR_LOG_ENRICHER_HPP
#define LUNAR_LOG_ENRICHER_HPP

#include "log_entry.hpp"
#include <functional>
#include <string>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// windows.h defines ERROR as 0, which conflicts with LogLevel::ERROR.
#ifdef ERROR
#undef ERROR
#endif
#else
#include <unistd.h>
#endif

namespace minta {

    /// Function-based enricher (preferred approach).
    /// Enrichers modify LogEntry::customContext to attach metadata.
    using EnricherFn = std::function<void(LogEntry&)>;

    /// Interface for polymorphic enrichers.
    /// Users who prefer class-based enrichers can implement this and
    /// wrap it in an EnricherFn via a lambda or std::shared_ptr capture.
    class IEnricher {
    public:
        virtual ~IEnricher() = default;
        virtual void enrich(LogEntry& entry) const = 0;
    };

    /// Built-in enrichers that attach common metadata to log entries.
    ///
    /// Static enrichers (property, fromEnv, machineName, environment,
    /// processId) evaluate ONCE at registration time and cache the result.
    /// Dynamic enrichers (threadId, caller) evaluate per log entry.
    namespace Enrichers {

        /// Attaches 'threadId' — the logging thread's ID as a string.
        /// Uses the thread ID already captured in the LogEntry, so the
        /// value reflects the thread that called the log method.
        inline EnricherFn threadId() {
            return [](LogEntry& entry) {
                // Convert thread::id to string without ostringstream to
                // avoid a heap allocation on every log call.
                std::hash<std::thread::id> hasher;
                entry.customContext["threadId"] = std::to_string(hasher(entry.threadId));
            };
        }

        /// Attaches 'processId' — the current process ID, cached at
        /// enricher creation time (PID is constant for process lifetime).
        inline EnricherFn processId() {
#ifdef _WIN32
            std::string cached = std::to_string(
                static_cast<unsigned long>(GetCurrentProcessId()));
#else
            std::string cached = std::to_string(
                static_cast<long>(getpid()));
#endif
            return [cached](LogEntry& entry) {
                entry.customContext["processId"] = cached;
            };
        }

        /// Attaches 'machine' — the hostname, cached at registration.
        inline EnricherFn machineName() {
            std::string cached;
#ifdef _WIN32
            char buf[MAX_COMPUTERNAME_LENGTH + 1];
            DWORD size = sizeof(buf);
            if (GetComputerNameA(buf, &size)) {
                cached = buf;
            }
#else
            // POSIX guarantees HOST_NAME_MAX >= 255. We use 256 to cover
            // all common platforms (Linux default 64, macOS 256). If
            // gethostname() fails or truncates, we fall back to empty string.
            char buf[256];
            if (gethostname(buf, sizeof(buf)) == 0) {
                buf[sizeof(buf) - 1] = '\0';
                cached = buf;
            }
#endif
            return [cached](LogEntry& entry) {
                entry.customContext["machine"] = cached;
            };
        }

        /// Attaches 'environment' — from $APP_ENV then $ENVIRONMENT,
        /// cached at registration. Falls back to empty string.
        inline EnricherFn environment() {
            std::string cached;
            const char* val = std::getenv("APP_ENV");
            if (!val) val = std::getenv("ENVIRONMENT");
            if (val) cached = val;
            return [cached](LogEntry& entry) {
                entry.customContext["environment"] = cached;
            };
        }

        /// Attaches a static key-value pair, evaluated once at registration.
        inline EnricherFn property(const std::string& key, const std::string& value) {
            return [key, value](LogEntry& entry) {
                entry.customContext[key] = value;
            };
        }

        /// Attaches a value from an environment variable, cached at registration.
        /// @param envVar  The environment variable name to read.
        /// @param key     The context key to store the value under.
        inline EnricherFn fromEnv(const std::string& envVar, const std::string& key) {
            const char* val = std::getenv(envVar.c_str());
            std::string cached = val ? val : "";
            return [key, cached](LogEntry& entry) {
                entry.customContext[key] = cached;
            };
        }

        /// Attaches 'caller' — the function name from source location.
        /// Only sets the key if source location capture is enabled and
        /// the function name is non-empty.
        inline EnricherFn caller() {
            return [](LogEntry& entry) {
                if (!entry.function.empty()) {
                    entry.customContext["caller"] = entry.function;
                }
            };
        }

    } // namespace Enrichers
} // namespace minta

#endif // LUNAR_LOG_ENRICHER_HPP
