#ifndef LUNAR_LOG_CONSOLE_SINK_HPP
#define LUNAR_LOG_CONSOLE_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/stdout_transport.hpp"

namespace minta {
    class ConsoleSink : public BaseSink {
    public:
        ConsoleSink() {
            setFormatter(detail::make_unique<HumanReadableFormatter>());
            setTransport(detail::make_unique<StdoutTransport>());
        }
    };
} // namespace minta

#endif // LUNAR_LOG_CONSOLE_SINK_HPP
