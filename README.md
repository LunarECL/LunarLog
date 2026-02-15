# LunarLog

Header-only C++ logging library with pluggable formatters, sinks, and transports. Works with C++11/14/17.

[![CI](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml/badge.svg)](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml)

## Features

- Log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- [Message Templates](https://messagetemplates.org/) with named placeholders
- Built-in formatters: human-readable, JSON, XML
- Multiple sinks (console, file) with independent formatters
- Thread-safe logging with atomic operations
- Rate limiting to prevent log flooding
- Context capture (global + scoped)
- Escaped brackets (`{{like this}}`)

## Quick Start

```cpp
#include "lunar_log.hpp"

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    logger.addSink<minta::ConsoleSink>();
    logger.addSink<minta::FileSink>("app.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("app.json.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("app.xml.log");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    logger.info("Escaped: {{literal braces}} {placeholder}", "value");

    return 0;
}
```

## Installation

Header-only — just add `include/` to your include path:

```cmake
add_subdirectory(LunarLog)
target_link_libraries(YourTarget PRIVATE LunarLog)
```

**Single header** — drop `single_include/lunar_log.hpp` into your project:
```cpp
#include "lunar_log.hpp"
```

To regenerate: `python3 tools/generate_single_header.py`

**Multi-file** — or use the full `include/` tree:
```
include/
└── lunar_log/
    ├── core/          # log entries, common utilities
    ├── formatter/     # human-readable, JSON, XML
    ├── sink/          # console, file
    ├── transport/     # stdout, file I/O
    ├── log_manager.hpp
    └── log_source.hpp
```

## Custom Formatters

```cpp
class MyFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry &entry) const override {
        return "[" + minta::getLevelString(entry.level) + "] " + entry.message + "\n";
    }
};

logger.addSink<minta::FileSink, MyFormatter>("custom.log");
```

## Suffix Formatting

Placeholders support format specifiers after a colon: `{name:spec}`.

| Specifier | Example | Output | Description |
|-----------|---------|--------|-------------|
| `.Nf` | `{val:.2f}` | `3.14` | Fixed-point with N decimal places |
| `Nf` | `{val:4f}` | `3.1416` | Shorthand fixed-point |
| `C` / `c` | `{val:C}` | `$42.50` | Currency format |
| `X` / `x` | `{val:X}` | `FF` | Hexadecimal (upper/lower) |
| `E` / `e` | `{val:e}` | `1.234568e+04` | Scientific notation |
| `P` / `p` | `{val:P}` | `85.60%` | Percentage (value * 100) |
| `0N` | `{val:04}` | `0042` | Zero-padded integer |

```cpp
logger.info("Price: {amount:C}, Discount: {pct:P}", 42.5, 0.15);
// => Price: $42.50, Discount: 15.00%
```

Non-numeric values ignore numeric format specifiers and render as-is.

## Context Capture

```cpp
logger.setCaptureSourceLocation(true);
logger.setContext("session_id", "abc123");
logger.info("request received");

{
    minta::ContextScope scope(logger, "request_id", "req-456");
    logger.info("processing within scope");
}
// scope context automatically removed
```

## Building & Testing

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLUNARLOG_CXX_STANDARD=17
cmake --build build
cd build && ctest --output-on-failure
```

Supported compilers:
- GCC (C++11, 14, 17)
- Clang / AppleClang (C++17)
- MSVC (C++17 required — tests use `<filesystem>`)

## CI

Runs on every push — build matrix across GCC/Clang/MSVC, plus static analysis (cppcheck, clang-tidy) and test coverage.

## License

MIT — see [LICENSE](LICENSE).
