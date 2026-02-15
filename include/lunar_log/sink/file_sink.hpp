#ifndef LUNAR_LOG_FILE_SINK_HPP
#define LUNAR_LOG_FILE_SINK_HPP

#include "sink_interface.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include "../transport/file_transport.hpp"

namespace minta {
    class FileSink : public BaseSink {
    public:
        explicit FileSink(const std::string &filename) {
            setFormatter(detail::make_unique<HumanReadableFormatter>());
            setTransport(detail::make_unique<FileTransport>(filename));
        }
    };
} // namespace minta

#endif // LUNAR_LOG_FILE_SINK_HPP
