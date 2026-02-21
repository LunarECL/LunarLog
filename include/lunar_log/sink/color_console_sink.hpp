#ifndef LUNAR_LOG_COLOR_CONSOLE_SINK_HPP
#define LUNAR_LOG_COLOR_CONSOLE_SINK_HPP

#include "sink_interface.hpp"
#include "console_sink.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/stdout_transport.hpp"
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// windows.h defines ERROR as 0, which conflicts with LogLevel::ERROR.
#ifdef ERROR
#undef ERROR
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

namespace minta {
    /// ANSI-color-enabled console sink.
    ///
    /// Drop-in alternative to ConsoleSink that colorizes the [LEVEL] bracket
    /// in log output. The message body is left uncolored.
    ///
    /// Color is auto-disabled when:
    ///   - stdout is not a TTY (piped / redirected)
    ///   - `NO_COLOR` environment variable is set (any value — https://no-color.org/)
    ///   - `LUNAR_LOG_NO_COLOR` environment variable is set (non-empty)
    ///
    /// On Windows 10+, ENABLE_VIRTUAL_TERMINAL_PROCESSING is activated
    /// automatically so that ANSI escape codes render correctly.
    class ColorConsoleSink : public BaseSink {
    public:
        explicit ColorConsoleSink(ConsoleStream stream = ConsoleStream::StdOut) {
            setFormatter(detail::make_unique<HumanReadableFormatter>());
            if (stream == ConsoleStream::StdOut) {
                setTransport(detail::make_unique<StdoutTransport>());
            } else {
                setTransport(detail::make_unique<StderrTransport>());
            }
            m_colorEnabled.store(detectAndEnableColorSupport(stream), std::memory_order_relaxed);
        }

        /// Override color auto-detection.
        void setColor(bool enabled) { m_colorEnabled.store(enabled, std::memory_order_relaxed); }

        /// Returns true if color output is currently enabled.
        bool isColorEnabled() const { return m_colorEnabled.load(std::memory_order_relaxed); }

        // Overrides BaseSink::write() because colorization must be injected
        // between format() and transport(). BaseSink has no virtual hook for
        // post-format transforms, so a full override is required.
        void write(const LogEntry& entry) override {
            IFormatter* fmt = formatter();
            ITransport* tp = transport();
            if (fmt && tp) {
                std::string formatted = fmt->format(entry);
                if (m_colorEnabled.load(std::memory_order_relaxed)) {
                    formatted = colorize(formatted, entry.level);
                }
                tp->write(formatted);
            }
        }

        /// Insert ANSI color codes around the [LEVEL] bracket in formatted text.
        /// Only the bracket (e.g. "[INFO]") is colorized; the message body is
        /// left uncolored.  Targets the **first** occurrence of "[LEVEL]" in
        /// the string, which is correct for HumanReadableFormatter output.
        /// Custom formatters that place the bracket later (or whose message
        /// body reproduces it) may see unexpected colorization.
        /// If no bracket is found, the text is returned unchanged.
        /// Public and static for testability.
        static std::string colorize(const std::string& text, LogLevel level) {
            const char* levelStr = getLevelString(level);
            std::string bracket = "[";
            bracket += levelStr;
            bracket += "]";

            size_t pos = text.find(bracket);
            if (pos == std::string::npos) return text;

            const char* color = getColorCode(level);
            const char* reset = "\033[0m";

            std::string result;
            result.reserve(text.size() + 20);
            result.append(text, 0, pos);
            result += color;
            result += bracket;
            result += reset;
            result.append(text, pos + bracket.size(), std::string::npos);
            return result;
        }

        /// Return the ANSI escape code for a log level.
        static const char* getColorCode(LogLevel level) {
            switch (level) {
                case LogLevel::TRACE: return "\033[2m";      // dim
                case LogLevel::DEBUG: return "\033[36m";     // cyan
                case LogLevel::INFO:  return "\033[32m";     // green
                case LogLevel::WARN:  return "\033[33m";     // yellow
                case LogLevel::ERROR: return "\033[31m";     // red
                case LogLevel::FATAL: return "\033[1;31m";   // bold red
                default: return "";
            }
        }

    private:
        std::atomic<bool> m_colorEnabled;

        static bool detectAndEnableColorSupport(ConsoleStream stream) {
            // Env-var checks run per-instance so that a later-constructed sink
            // respects runtime changes to NO_COLOR / LUNAR_LOG_NO_COLOR.
            // The VT-mode setup below (Windows only) runs once per stream via
            // call_once because it is an idempotent OS call that is harmless
            // if left enabled even when a subsequent sink disables color.
            if (std::getenv("NO_COLOR") != nullptr) return false;

            const char* noColor = std::getenv("LUNAR_LOG_NO_COLOR");
            if (noColor && noColor[0] != '\0') return false;

#ifdef _WIN32
            static std::once_flag s_stdoutOnce;
            static std::once_flag s_stderrOnce;
            static std::atomic<bool> s_stdoutVt(false);
            static std::atomic<bool> s_stderrVt(false);

            std::once_flag& flag = (stream == ConsoleStream::StdOut)
                ? s_stdoutOnce : s_stderrOnce;
            std::atomic<bool>& result = (stream == ConsoleStream::StdOut)
                ? s_stdoutVt : s_stderrVt;

            std::call_once(flag, [&]() {
                DWORD handleType = (stream == ConsoleStream::StdOut)
                    ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
                HANDLE hOut = GetStdHandle(handleType);
                if (hOut == INVALID_HANDLE_VALUE) return;
                DWORD mode = 0;
                if (!GetConsoleMode(hOut, &mode)) return;
                if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                    if (!SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                        return;
                    }
                }
                result.store(true, std::memory_order_relaxed);
            });

            return result.load(std::memory_order_relaxed);
#else
            FILE* fp = (stream == ConsoleStream::StdOut) ? stdout : stderr;
            return isatty(fileno(fp)) != 0;
#endif
        }
    };
} // namespace minta

#endif // LUNAR_LOG_COLOR_CONSOLE_SINK_HPP
