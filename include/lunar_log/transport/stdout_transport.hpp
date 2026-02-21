#ifndef LUNAR_LOG_STDOUT_TRANSPORT_HPP
#define LUNAR_LOG_STDOUT_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <iostream>
#include <mutex>

namespace minta {
    /// @note All StdoutTransport instances share a single mutex so that
    ///       concurrent writes to stdout are serialized.  StderrTransport
    ///       has its own independent mutex, so stdout and stderr writes
    ///       may interleave at the terminal level.
    class StdoutTransport : public ITransport {
    public:
        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(sharedMutex());
            // Flush each line for immediate console visibility.
            std::cout << formattedEntry << '\n' << std::flush;
        }

    private:
        static std::mutex& sharedMutex() {
            static std::mutex s_mutex;
            return s_mutex;
        }
    };

    /// @note All StderrTransport instances share a single mutex so that
    ///       concurrent writes to stderr are serialized.  StdoutTransport
    ///       has its own independent mutex, so stdout and stderr writes
    ///       may interleave at the terminal level.
    class StderrTransport : public ITransport {
    public:
        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(sharedMutex());
            // Flush each line for immediate console visibility.
            std::cerr << formattedEntry << '\n' << std::flush;
        }

    private:
        static std::mutex& sharedMutex() {
            static std::mutex s_mutex;
            return s_mutex;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_STDOUT_TRANSPORT_HPP
