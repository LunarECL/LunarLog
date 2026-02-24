#ifndef LUNAR_LOG_CONFIG_WATCHER_HPP
#define LUNAR_LOG_CONFIG_WATCHER_HPP

#include "json_parser.hpp"
#include "log_level.hpp"
#include "filter_rule.hpp"
#include "compact_filter.hpp"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <fstream>
#include <chrono>
#include <iterator>
#include <stdexcept>

// Cross-platform file stat for mtime checking
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define LUNARLOG_WATCHER_USE_FILESYSTEM
#include <filesystem>
#else
#include <sys/stat.h>
#ifdef _MSC_VER
#include <sys/types.h>
#endif
#endif

namespace minta {
namespace detail {

    /// Parameters for config file watching, stored during build.
    struct ConfigWatcherParams {
        std::string path;
        std::chrono::seconds interval;
    };

    /// Polling-based configuration file watcher.
    ///
    /// Checks the mtime of a JSON configuration file at a configurable
    /// interval. When the file changes, it parses the JSON and invokes
    /// callbacks to apply the new configuration.
    ///
    /// Self-contained — uses std::function callbacks instead of direct
    /// LunarLog references to avoid circular header dependencies.
    ///
    /// Thread safety: the watcher runs its own polling thread. Callbacks
    /// are invoked from that thread and must be thread-safe.
    class ConfigWatcher {
    public:
        /// Callback invoked with the parsed JSON config when the file changes.
        using ApplyCallback = std::function<void(const JsonValue&)>;
        /// Callback invoked with a warning message on parse errors.
        using WarnCallback = std::function<void(const std::string&)>;

        ConfigWatcher(const std::string& path,
                      std::chrono::seconds interval,
                      ApplyCallback onConfigLoaded,
                      WarnCallback onWarning)
            : m_path(path)
            , m_interval(interval)
            , m_onConfigLoaded(std::move(onConfigLoaded))
            , m_onWarning(std::move(onWarning))
            , m_running(false)
#ifndef LUNARLOG_WATCHER_USE_FILESYSTEM
            , m_lastMtime(0)
#endif
            {}

        ~ConfigWatcher() {
            stop();
        }

        ConfigWatcher(const ConfigWatcher&) = delete;
        ConfigWatcher& operator=(const ConfigWatcher&) = delete;

        /// Start the polling thread. Performs an initial config load.
        /// Safe to call only once; subsequent calls are a no-op.
        /// Thread-safe: serialized with stop() via m_lifecycleMutex.
        void start() {
            std::lock_guard<std::mutex> lock(m_lifecycleMutex);
            bool expected = false;
            if (!m_running.compare_exchange_strong(
                    expected, true, std::memory_order_acq_rel)) {
                return; // already running
            }
            m_thread = std::thread(&ConfigWatcher::pollLoop, this);
        }

        /// Stop the polling thread and wait for it to finish.
        /// Thread-safe: serialized with start() via m_lifecycleMutex.
        void stop() {
            std::lock_guard<std::mutex> lock(m_lifecycleMutex);
            m_running.store(false, std::memory_order_release);
            if (m_thread.joinable()) {
                m_thread.join();
            }
        }

    private:
        std::string m_path;
        std::chrono::seconds m_interval;
        ApplyCallback m_onConfigLoaded;
        WarnCallback m_onWarning;
        std::mutex m_lifecycleMutex;
        std::thread m_thread;
        std::atomic<bool> m_running;
#ifdef LUNARLOG_WATCHER_USE_FILESYSTEM
        std::filesystem::file_time_type m_lastFtime;
#else
        long long m_lastMtime;
#endif

        void pollLoop() {
            // Initial config load on startup
            checkAndApply();

            while (m_running.load(std::memory_order_acquire)) {
                // Sleep in 100ms increments so we can exit quickly on stop()
                long long totalMs = m_interval.count() * 1000;
                long long slept = 0;
                while (slept < totalMs &&
                       m_running.load(std::memory_order_relaxed)) {
                    long long chunk = (totalMs - slept < 100)
                                      ? (totalMs - slept) : 100;
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(chunk));
                    slept += chunk;
                }
                if (!m_running.load(std::memory_order_relaxed)) break;
                checkAndApply();
            }
        }

        void checkAndApply() {
            if (!hasFileChanged()) return;

            // Read file contents
            std::ifstream file(m_path.c_str());
            if (!file.is_open()) {
                if (m_onWarning) {
                    m_onWarning("Config watcher: cannot open file '" +
                                m_path + "'");
                }
                return;
            }
            std::string content(
                (std::istreambuf_iterator<char>(file)),
                 std::istreambuf_iterator<char>());

            // Parse JSON
            JsonValue config;
            try {
                config = JsonValue::parse(content);
            } catch (const std::exception& ex) {
                // Malformed JSON — keep current settings, log warning
                if (m_onWarning) {
                    m_onWarning(
                        std::string("Config reload failed (keeping current "
                                    "settings): ") + ex.what());
                }
                return;
            }

            // Apply configuration
            if (m_onConfigLoaded) {
                m_onConfigLoaded(config);
            }
        }

        bool hasFileChanged() {
#ifdef LUNARLOG_WATCHER_USE_FILESYSTEM
            std::error_code ec;
            auto ftime = std::filesystem::last_write_time(m_path, ec);
            if (ec) return false;
            if (ftime == m_lastFtime) return false;
            m_lastFtime = ftime;
            return true;
#else
            struct stat st;
            if (stat(m_path.c_str(), &st) != 0) return false;
            long long mtime = static_cast<long long>(st.st_mtime);
            if (mtime == m_lastMtime) return false;
            m_lastMtime = mtime;
            return true;
#endif
        }
    };

    /// Parse a log level string (case-insensitive).
    /// Returns true on success, false on invalid input.
    inline bool tryParseLogLevel(const std::string& s, LogLevel& out) {
        std::string upper;
        upper.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            upper += static_cast<char>(
                (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
        }
        if (upper == "TRACE") { out = LogLevel::TRACE; return true; }
        if (upper == "DEBUG") { out = LogLevel::DEBUG; return true; }
        if (upper == "INFO")  { out = LogLevel::INFO;  return true; }
        if (upper == "WARN")  { out = LogLevel::WARN;  return true; }
        if (upper == "ERROR") { out = LogLevel::ERROR;  return true; }
        if (upper == "FATAL") { out = LogLevel::FATAL;  return true; }
        return false;
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_CONFIG_WATCHER_HPP
