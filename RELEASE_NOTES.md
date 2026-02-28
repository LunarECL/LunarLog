# LunarLog v1.29.2 — Release Notes

## shared_mutex for hot-path locks

Converted 7 read-heavy mutexes across `log_source.hpp`, `sink_interface.hpp`, and `formatter_interface.hpp` from `std::mutex` to `std::shared_mutex` (C++17). On C++11/14, the library transparently falls back to `std::mutex` with identical behavior.

### What changed

- **Template cache**, **locale**, **context**, and **global filter** locks in the logger now use shared (reader) locks on the hot path
- **Per-sink filter** and **tag routing** locks also converted — these are called per log entry per sink
- **Per-formatter locale** lock converted
- Shared lock abstraction extracted to `core/shared_mutex.hpp`
- MSVC `_MSVC_LANG` detection ensures C++17 features activate correctly without `/Zc:__cplusplus`

### New SinkProxy accessors

- `clearOnlyTags()`, `clearExceptTags()` — clear tag routing filters individually
- `getOnlyTags()`, `getExceptTags()` — inspect current tag filters

### Tests

- 7 new tests covering sink filter and tag clear/get methods
