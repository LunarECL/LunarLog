#ifndef LUNAR_LOG_CONSOLE_SINK_HPP
#define LUNAR_LOG_CONSOLE_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/stdout_transport.hpp"

namespace minta {
    class ConsoleSink : public ISink {
    public:
        ConsoleSink() {
            setFormatter(make_unique<HumanReadableFormatter>());
            setTransport(make_unique<StdoutTransport>());
        }

        void write(const LogEntry &entry) override {
            if (m_formatter && m_transport) {
                m_transport->write(m_formatter->format(entry));
            }
        }
    };
} // namespace minta

#endif // LUNAR_LOG_CONSOLE_SINK_HPP
