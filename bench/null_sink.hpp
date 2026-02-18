#pragma once
#include "lunar_log/sink/sink_interface.hpp"

namespace minta {

class NullSink : public ISink {
public:
    void write(const LogEntry&) override {}
};

} // namespace minta
