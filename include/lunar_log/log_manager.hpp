#ifndef LUNAR_LOG_MANAGER_HPP
#define LUNAR_LOG_MANAGER_HPP

#include "sink/sink_interface.hpp"
#include <vector>
#include <memory>

namespace minta {
    class LogManager {
    public:
        void addSink(std::unique_ptr<ISink> sink) {
            m_sinks.push_back(std::move(sink));
        }

        void log(const LogEntry &entry) {
            for (const auto &sink: m_sinks) {
                sink->write(entry);
            }
        }

    private:
        std::vector<std::unique_ptr<ISink> > m_sinks;
    };
} // namespace minta

#endif // LUNAR_LOG_MANAGER_HPP
