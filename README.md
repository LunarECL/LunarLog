# LunarLog

**Header-only C++11 structured logging with [message templates](https://messagetemplates.org/). Inspired by [Serilog](https://serilog.net/).**

[![CI](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml/badge.svg)](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml)
[![Coverage](https://codecov.io/gh/LunarECL/LunarLog/branch/master/graph/badge.svg)](https://codecov.io/gh/LunarECL/LunarLog)
[![Release](https://img.shields.io/github/v/release/LunarECL/LunarLog)](https://github.com/LunarECL/LunarLog/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![C++11](https://img.shields.io/badge/C%2B%2B-11%2F14%2F17-blue.svg)

## Quick Start

```cpp
#include "lunar_log.hpp"
#include <lunar_log/macros.hpp>

int main() {
    // Fluent Builder — configure, add sinks, enrich, build
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .captureSourceLocation(true)
        .enrich(minta::Enrichers::threadId())
        .enrich(minta::Enrichers::property("service", "my-app"))
        .writeTo<minta::ConsoleSink>("console")
        .writeTo<minta::FileSink>("app-log", "app.log")
        .writeTo<minta::FileSink, minta::JsonFormatter>("json-out", "app.json.log")
        .build();

    // Message templates — named placeholders, structured properties
    logger.info("User {name} logged in from {ip}", "name", "alice", "ip", "10.0.0.1");

    // Source location macros — auto-capture __FILE__, __LINE__, __func__
    LUNAR_INFO(logger, "Order {id} processed: {items} items, total {total:.2f}",
               "id", "ORD-1234", "items", 3, "total", 59.97);

    return 0;
}
```

## Why LunarLog?

- **Structured logging** — [message templates](https://messagetemplates.org/) with named placeholders, not printf. Properties are preserved in JSON/XML for machine processing.
- **Zero dependencies** — header-only, no external libraries. Just `#include` and go.
- **C++11 and up** — works with GCC, Clang, and MSVC. No C++17 required.
- **Production-ready** — thread-safe, rate-limited, rolling file rotation, multi-layer filtering.
- **Serilog-inspired** — destructuring operators, enrichers, and fluent builder will feel familiar.

## Features

- [**Message Templates**](https://github.com/LunarECL/LunarLog/wiki/Message-Templates) — named & indexed placeholders, `@`/`$` operators, [format specifiers](https://github.com/LunarECL/LunarLog/wiki/Format-Specifiers)
- [**Output Formats**](https://github.com/LunarECL/LunarLog/wiki/Structured-Output) — human-readable, JSON, [Compact JSON (CLEF)](https://github.com/LunarECL/LunarLog/wiki/Compact-JSON-Formatter), XML
- [**Filtering**](https://github.com/LunarECL/LunarLog/wiki/Filtering) — per-sink levels, predicates, DSL rules, [compact syntax](https://github.com/LunarECL/LunarLog/wiki/Compact-Filter) (`"WARN+ ~timeout"`)
- [**Sinks**](https://github.com/LunarECL/LunarLog/wiki/Getting-Started) — Console, ColorConsole, File, [RollingFile](https://github.com/LunarECL/LunarLog/wiki/Rolling-File-Sink) — [named sinks & tag routing](https://github.com/LunarECL/LunarLog/wiki/Named-Sinks-and-Tag-Routing)
- [**Pipe Transforms**](https://github.com/LunarECL/LunarLog/wiki/Pipe-Transforms) — 18 built-in: `upper`, `trim`, `comma`, `bytes`, `duration`, `truncate`, chainable
- [**Enrichers**](https://github.com/LunarECL/LunarLog/wiki/Enrichers) — auto-attach ThreadId, ProcessId, MachineName, environment, custom lambdas
- [**Exception Attachment**](https://github.com/LunarECL/LunarLog/wiki/Exception-Attachment) — `logger.error(ex, "msg")` with nested exception unwinding
- [**Fluent Builder**](https://github.com/LunarECL/LunarLog/wiki/Fluent-Builder) — declarative `LunarLog::configure().writeTo<>().enrich().build()`
- [**Source Location Macros**](https://github.com/LunarECL/LunarLog/wiki/Source-Location-Macros) — `LUNAR_INFO(logger, ...)` auto-captures `__FILE__`, `__LINE__`, `__func__`
- [**Scoped Context**](https://github.com/LunarECL/LunarLog/wiki/Scoped-Context) — RAII `LogScope` for request-lifetime fields
- [**Thread Safety**](https://github.com/LunarECL/LunarLog/wiki/Thread-Safety), [**Rate Limiting**](https://github.com/LunarECL/LunarLog/wiki/Rate-Limiting) — production-safe concurrency

## Installation

**Single header** — download and include:

```bash
wget https://raw.githubusercontent.com/LunarECL/LunarLog/master/single_include/lunar_log.hpp
```

**CMake FetchContent:**

```cmake
include(FetchContent)
FetchContent_Declare(LunarLog GIT_REPOSITORY https://github.com/LunarECL/LunarLog.git GIT_TAG v1.23.0)
FetchContent_MakeAvailable(LunarLog)
target_link_libraries(YourTarget PRIVATE LunarLog)
```

**CMake subdirectory:**

```cmake
add_subdirectory(LunarLog)
target_link_libraries(YourTarget PRIVATE LunarLog)
```

**vcpkg** *(pending official registry acceptance)*:

```bash
vcpkg install lunarlog --overlay-ports=./ports
```

Once in the official registry: `vcpkg install lunarlog`

**Conan** *(pending ConanCenter acceptance)*:

```bash
conan install --requires="lunarlog/1.23.0"
```

Full installation guide: [Getting Started](https://github.com/LunarECL/LunarLog/wiki/Getting-Started)

## Documentation

| Guide | Description |
|-------|-------------|
| [Getting Started](https://github.com/LunarECL/LunarLog/wiki/Getting-Started) | Installation, first log, build setup |
| [API Reference](https://github.com/LunarECL/LunarLog/wiki/API-Reference) | Every public method with signatures and examples |
| [Fluent Builder](https://github.com/LunarECL/LunarLog/wiki/Fluent-Builder) | Declarative logger configuration |
| [Message Templates](https://github.com/LunarECL/LunarLog/wiki/Message-Templates) | Placeholders, operators, format specifiers |
| [Filtering](https://github.com/LunarECL/LunarLog/wiki/Filtering) | Per-sink levels, predicates, DSL rules |
| [Enrichers](https://github.com/LunarECL/LunarLog/wiki/Enrichers) | Auto-attach metadata to every log entry |
| [Cookbook](https://github.com/LunarECL/LunarLog/wiki/Cookbook) | Real-world recipes and patterns |
| [Benchmark Suite](https://github.com/LunarECL/LunarLog/wiki/Benchmark-Suite) | Performance benchmarks and CI integration |

[Full wiki](https://github.com/LunarECL/LunarLog/wiki) · [Examples](examples/) · [Contributing](CONTRIBUTING.md)

## Performance

Benchmarks run in CI on every push (Release mode, Google Benchmark). The suite measures throughput, formatting, filtering, sink I/O, enrichers, and end-to-end latency.

[Benchmark Suite](https://github.com/LunarECL/LunarLog/wiki/Benchmark-Suite)

## License

MIT — see [LICENSE](LICENSE).
