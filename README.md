# LunarLog

Header-only C++ logging library with pluggable formatters, sinks, and transports. Works with C++11/14/17.

[![CI](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml/badge.svg)](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml)

## Features

- Log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- [Message Templates](https://messagetemplates.org/) with named placeholders
- **`@` and `$` operators** — `{@val}` for destructuring, `{$val}` for stringify
- Built-in formatters: human-readable, JSON, XML
- Multiple sinks (console, file) with independent formatters
- **Structured template output** — JSON/XML include raw template + hash for log aggregation
- **Template caching** — parsed templates are cached for repeated use
- Thread-safe logging with atomic operations
- Rate limiting to prevent log flooding
- Context capture (global + scoped)
- Escaped brackets (`{{like this}}`)
- Format specifiers: `{val:.2f}`, `{x:X}`, `{amt:C}`, `{rate:P}`, `{id:04}`, `{v:e}`

## Format Specifiers

Append a format spec after a colon in any placeholder:

```cpp
logger.info("Price: {amount:.2f}", 3.14159);   // Price: 3.14
logger.info("Hex: {val:X}", 255);               // Hex: FF
logger.info("Cost: {price:C}", 9.99);           // Cost: $9.99
logger.info("Rate: {r:P}", 0.856);              // Rate: 85.60%
logger.info("ID: {id:04}", 42);                 // ID: 0042
logger.info("Sci: {v:e}", 1500.0);              // Sci: 1.500000e+03
```

| Spec | Description | Example |
|---|---|---|
| `.Nf` | Fixed-point with N decimals | `{:.2f}` → `3.14` |
| `X` / `x` | Uppercase / lowercase hex | `{:X}` → `FF` |
| `C` / `c` | Currency ($) | `{:C}` → `$9.99` |
| `P` / `p` | Percentage (×100) | `{:P}` → `85.60%` |
| `E` / `e` | Scientific notation | `{:e}` → `1.5e+03` |
| `0N` | Zero-padded integer | `{:04}` → `0042` |

Namespaced names work — the spec splits on the last colon: `{ns:key:.2f}`.

## Operators (`@` and `$`)

Per the [MessageTemplates](https://messagetemplates.org/) spec, placeholders support operator prefixes:

- **`@` (destructure)** — hints structured formatters to preserve the value's native type
- **`$` (stringify)** — forces the value to be captured as a string

```cpp
logger.info("User: {@id}, Tag: {$label}", 42, true);
```

**In JSON output**, `@` triggers type detection — numbers and booleans are emitted as native JSON types, while `$` always emits strings:

```json
{"message":"User: 42, Tag: true","properties":{"id":42,"label":"true"}}
```

**In XML output**, properties get `destructure="true"` or `stringify="true"` attributes.

**In human-readable output**, operators are transparent — the message looks the same either way.

Operators combine with format specifiers: `{@amount:.2f}` formats the message as `3.14` but stores the raw value (`3.14159`) in properties.

Invalid forms like `{@}`, `{@@x}`, `{@$x}`, and `{@ }` are treated as literal text.

## Structured Template Output

JSON and XML formatters include the raw message template alongside the rendered message. This enables log aggregation tools (Seq, Elasticsearch, Splunk) to group entries by template.

```cpp
logger.addSink<minta::FileSink, minta::JsonFormatter>("structured.log");
logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
```

**JSON output:**
```json
{
  "level": "INFO",
  "timestamp": "2026-02-16T12:00:00.000Z",
  "messageTemplate": "User {username} logged in from {ip}",
  "templateHash": "a1b2c3d4",
  "message": "User alice logged in from 192.168.1.1",
  "properties": { "username": "alice", "ip": "192.168.1.1" }
}
```

The `templateHash` is an FNV-1a hash (8-char hex) for efficient grouping without string comparison.

### Template Cache

Parsed templates are cached to avoid re-parsing on repeated log calls. The cache holds up to 128 entries by default — once full, existing entries stay cached and new templates are parsed on every call.

```cpp
logger.setTemplateCacheSize(256);  // increase cache
logger.setTemplateCacheSize(0);    // disable caching
```

### Placeholder Validation

LunarLog warns about common template mistakes:
- **Duplicate names:** `{name} and {name}` — same name appears twice
- **Empty placeholders:** `{}`
- **Whitespace-only names:** `{ }`

## Quick Start

```cpp
#include "lunar_log.hpp"

int main() {
    // Constructor adds a ConsoleSink by default; no need to add one manually.
    minta::LunarLog logger(minta::LogLevel::TRACE);

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

## Custom Sinks

You can add a fully custom sink via `addCustomSink`:

```cpp
class MySink : public minta::ISink {
public:
    MySink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }
    void write(const minta::LogEntry &entry) override {
        std::cout << formatter()->format(entry) << std::endl;
    }
};

auto sink = minta::detail::make_unique<MySink>();
logger.addCustomSink(std::move(sink));
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

## Rate Limiting

LunarLog enforces a hardcoded rate limit of **1000 messages per second** per logger instance. Messages exceeding the limit within a 1-second window are silently dropped. The window resets automatically after 1 second of wall-clock time. Validation warnings generated by placeholder mismatches are exempt from rate limiting.

## Thread Safety

LunarLog is designed for concurrent use from multiple threads with the following contract:

- **Add all sinks before logging.** Calling `addSink()` after the first log message has been processed throws `std::logic_error`. Configure sinks during initialization only.
- **LunarLog must outlive all logging threads.** Destroying a `LunarLog` instance while other threads are still calling log methods is undefined behavior. Join or stop all producer threads before the logger goes out of scope.
- **Do not call `setFormatter`/`setTransport` after sink registration.** These methods are `protected` on `ISink` and should only be called from sink constructors. The `LunarLog` class accesses them internally via `friend` during `addSink<SinkType, FormatterType>(...)`.
- **`setContext` / `clearContext` / `clearAllContext`** are thread-safe and can be called concurrently with logging.
- **`setMinLevel` / `setCaptureSourceLocation`** are atomic and safe to call at any time.
- **`ContextScope`** must not outlive the `LunarLog` instance it references. It holds a reference to the logger and calls `clearContext` in its destructor, so destroying the logger first is undefined behavior.

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
