#ifndef LUNAR_LOG_FILE_SINK_HPP
#define LUNAR_LOG_FILE_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/file_transport.hpp"

namespace minta {
    class FileSink : public ISink {
    public:
        FileSink(const std::string &filename) {
            setFormatter(make_unique<HumanReadableFormatter>());
            setTransport(make_unique<FileTransport>(filename));
        }

        void write(const LogEntry &entry) override {
            if (m_formatter && m_transport) {
                m_transport->write(m_formatter->format(entry));
            }
        }
    };
} // namespace minta

#endif // LUNAR_LOG_FILE_SINK_HPP
