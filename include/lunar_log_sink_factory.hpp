#ifndef LUNAR_LOG_SINK_FACTORY_HPP
#define LUNAR_LOG_SINK_FACTORY_HPP

#include "lunar_log_sink_interface.hpp"
#include "lunar_log_console_sink.hpp"
#include "lunar_log_file_sink.hpp"
#include <memory>

namespace minta {

    class SinkFactory {
    public:
        template<typename SinkType, typename... Args>
        static std::unique_ptr<SinkType> createSink(Args&&... args) {
            return minta::make_unique<SinkType>(std::forward<Args>(args)...);
        }
    };

} // namespace minta

#endif // LUNAR_LOG_SINK_FACTORY_HPP