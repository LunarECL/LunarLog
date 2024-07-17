#ifndef LUNAR_LOG_FORMATTER_INTERFACE_HPP
#define LUNAR_LOG_FORMATTER_INTERFACE_HPP

#include "../core/log_entry.hpp"
#include <string>

namespace minta {
    class IFormatter {
    public:
        virtual ~IFormatter() = default;

        virtual std::string format(const LogEntry &entry) const = 0;
    };
} // namespace minta

#endif // LUNAR_LOG_FORMATTER_INTERFACE_HPP
