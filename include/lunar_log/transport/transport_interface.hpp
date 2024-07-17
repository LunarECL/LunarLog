#ifndef LUNAR_LOG_TRANSPORT_INTERFACE_HPP
#define LUNAR_LOG_TRANSPORT_INTERFACE_HPP

#include <string>

namespace minta {

    class ITransport {
    public:
        virtual ~ITransport() = default;
        virtual void write(const std::string& formattedEntry) = 0;
    };

} // namespace minta

#endif // LUNAR_LOG_TRANSPORT_INTERFACE_HPP