#ifndef LUNAR_LOG_CONSOLE_SINK_HPP
#define LUNAR_LOG_CONSOLE_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/stdout_transport.hpp"

namespace minta {

    /// Selects which standard stream a console sink writes to.
    enum class ConsoleStream { StdOut, StdErr };

    class ConsoleSink : public BaseSink {
    public:
        explicit ConsoleSink(ConsoleStream stream = ConsoleStream::StdOut) {
            setFormatter(detail::make_unique<HumanReadableFormatter>());
            if (stream == ConsoleStream::StdOut) {
                setTransport(detail::make_unique<StdoutTransport>());
            } else {
                setTransport(detail::make_unique<StderrTransport>());
            }
        }
    };
} // namespace minta

#endif // LUNAR_LOG_CONSOLE_SINK_HPP
