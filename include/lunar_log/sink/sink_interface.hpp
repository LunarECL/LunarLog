#ifndef LUNAR_LOG_SINK_INTERFACE_HPP
#define LUNAR_LOG_SINK_INTERFACE_HPP

#include "../core/log_entry.hpp"
#include "../formatter/formatter_interface.hpp"
#include "../transport/transport_interface.hpp"
#include <memory>

namespace minta {
    class LunarLog;

    class ISink {
    public:
        virtual ~ISink() = default;

        virtual void write(const LogEntry &entry) = 0;

    protected:
        void setFormatter(std::unique_ptr<IFormatter> formatter) {
            m_formatter = std::move(formatter);
        }

        void setTransport(std::unique_ptr<ITransport> transport) {
            m_transport = std::move(transport);
        }

        std::unique_ptr<IFormatter> m_formatter;
        std::unique_ptr<ITransport> m_transport;

        friend class LunarLog;
    };
} // namespace minta

#endif // LUNAR_LOG_SINK_INTERFACE_HPP
