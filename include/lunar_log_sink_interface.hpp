#ifndef LUNAR_LOG_SINK_INTERFACE_HPP
#define LUNAR_LOG_SINK_INTERFACE_HPP

#include "lunar_log_common.hpp"

namespace minta {

    class ILogSink {
    public:
        virtual ~ILogSink() = default;
        virtual void write(const LogEntry& entry) = 0;
    };

} // namespace minta

#endif // LUNAR_LOG_SINK_INTERFACE_HPP