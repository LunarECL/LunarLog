#ifndef LUNAR_LOG_STDOUT_TRANSPORT_HPP
#define LUNAR_LOG_STDOUT_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <iostream>
#include <mutex>

namespace minta {
    class StdoutTransport : public ITransport {
    public:
        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(sharedMutex());
            std::cout << formattedEntry << '\n' << std::flush;
        }

    private:
        static std::mutex& sharedMutex() {
            static std::mutex s_mutex;
            return s_mutex;
        }
    };

    class StderrTransport : public ITransport {
    public:
        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(sharedMutex());
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
