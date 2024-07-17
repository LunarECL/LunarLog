#ifndef LUNAR_LOG_STDOUT_TRANSPORT_HPP
#define LUNAR_LOG_STDOUT_TRANSPORT_HPP

#include "transport_interface.hpp"
#include <iostream>
#include <mutex>

namespace minta {
    class StdoutTransport : public ITransport {
    public:
        void write(const std::string &formattedEntry) override {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::cout << formattedEntry << std::endl;
        }

    private:
        std::mutex m_mutex;
    };
} // namespace minta

#endif // LUNAR_LOG_STDOUT_TRANSPORT_HPP
