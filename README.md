# LunarLog

Header-only C++ logging library with pluggable formatters, sinks, and transports. Works with C++11/14/17.

[![CI](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml/badge.svg)](https://github.com/LunarECL/LunarLog/actions/workflows/ci.yml)

## Documentation

- **[Examples](examples/)** — compilable examples covering every feature
- **[Contributing Guide](CONTRIBUTING.md)** — build instructions, code style, PR process
- **[GitHub Wiki](https://github.com/LunarECL/LunarLog/wiki)** — extended documentation and guides

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
- **Named sinks** — register, look up, and configure sinks by name with `named()` and `SinkProxy`
- **Tag routing** — `[tag]` prefix parsing with `only()`/`except()` per-sink filtering
- **Filtering** — per-sink levels, predicate filters, DSL filter rules
- Escaped brackets (`{{like this}}`)
- Format specifiers: `{val:.2f}`, `{x:X}`, `{amt:C}`, `{rate:P}`, `{id:04}`, `{v:e}`
- **Pipe transforms** — `{name|upper}`, `{name|comma}`, `{name|truncate:20}` — 18 built-in transforms with chaining
- **Culture-specific formatting** — locale-aware numbers (`{:n}`), dates and times (`{:d}`, `{:T}`, etc.)

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

## Culture-Specific Formatting

Set a locale on the logger to enable locale-aware number and date/time formatting:

```cpp
logger.setLocale("de_DE");
logger.info("Total: {amount:n}", 1234567.89);   // Total: 1.234.567,89
logger.info("Date: {ts:D}", 1705312245);         // Date: Montag, Januar 15, 2024
```

| Spec | Description | Example (en_US) |
|---|---|---|
| `n` / `N` | Locale-aware number (thousand/decimal separators) | `{:n}` → `1,234,567.89` |
| `d` | Short date | `{:d}` → `01/15/2024` |
| `D` | Long date | `{:D}` → `Monday, January 15, 2024` |
| `t` | Short time | `{:t}` → `10:30` |
| `T` | Long time | `{:T}` → `10:30:45` |
| `f` | Full date + short time | `{:f}` → `Monday, January 15, 2024 10:30` |
| `F` | Full date + long time | `{:F}` → `Monday, January 15, 2024 10:30:45` |

Date/time specs expect the value to be a Unix timestamp (seconds since epoch). Non-numeric values are returned as-is.

The default locale is `"C"` (no culture-specific formatting). If a locale is not available on the system, it falls back to `"C"` automatically.

### Per-Sink Locale

Override the locale for individual sinks. The per-sink locale re-renders the message using that locale instead of the logger-level locale:

```cpp
logger.setLocale("en_US");
logger.addSink<minta::FileSink>("report_de.log");
logger.setSinkLocale(1, "de_DE");  // sink 1 uses German formatting

logger.info("Total: {val:n}", 1234567.89);
// sink 0 (en_US): Total: 1,234,567.89
// sink 1 (de_DE): Total: 1.234.567,89
```

### Thread Safety

`setLocale()` and `setSinkLocale()` are thread-safe and can be called concurrently with logging.

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

## Named Sinks

Give sinks human-readable names with `named()` instead of tracking indices. Use `SinkProxy` to configure them fluently:

```cpp
minta::LunarLog logger(minta::LogLevel::TRACE, false);

// Register sinks with names
logger.addSink<minta::ConsoleSink>(minta::named("console"));
logger.addSink<minta::FileSink>(minta::named("app-log"), "app.log");
logger.addSink<minta::FileSink, minta::JsonFormatter>(minta::named("json-out"), "app.json.log");

// Configure via SinkProxy — fluent, chainable API
logger.sink("json-out")
    .level(minta::LogLevel::INFO)
    .filterRule("not message contains 'heartbeat'")
    .locale("de_DE");

// Look up and reconfigure at runtime
logger.sink("console").level(minta::LogLevel::WARN);

// Clear filters on a named sink
logger.sink("json-out").clearFilters();
```

Unnamed sinks are auto-named `"sink_0"`, `"sink_1"`, etc. The `SinkProxy` also supports `only()`/`except()` for tag routing (see below), `filter()` for predicate filters, and `formatter()` for changing the formatter before logging starts.

## Tag Routing

Prefix messages with `[tag]` to categorize them. Tags are parsed, stripped from the rendered message, and used for per-sink routing:

```cpp
minta::LunarLog logger(minta::LogLevel::INFO, false);

logger.addSink<minta::ConsoleSink>(minta::named("console"));
logger.addSink<minta::FileSink>(minta::named("auth-log"), "auth.log");
logger.addSink<minta::FileSink>(minta::named("main-log"), "main.log");

// auth-log only receives messages tagged [auth]
logger.sink("auth-log").only("auth");
// main-log receives everything except [health] messages
logger.sink("main-log").except("health");

logger.info("[auth] User {name} logged in", "alice");
// -> console ✓, auth-log ✓ (matches "auth"), main-log ✓

logger.info("[health] Heartbeat OK");
// -> console ✓, auth-log ✗ (not "auth"), main-log ✗ (excepts "health")

logger.info("General message");
// -> console ✓, auth-log ✗ (untagged → excluded by only()), main-log ✓
```

**Multi-tag:** Adjacent brackets parse multiple tags — `[auth][security] message` has two tags. A space breaks the scan: `[auth] [security] msg` only parses `auth`.

**JSON output** includes a `"tags"` array:
```json
{"level":"INFO","message":"User alice logged in","tags":["auth"],"properties":{...}}
```

**Tag routing runs before** per-sink level and DSL filters in the pipeline: global filters → tag routing → per-sink level → per-sink filters.

## Pipe Transforms

Apply post-format transformations to placeholder values with the `|` (pipe) operator:

```cpp
logger.info("User: {name|upper}", "alice");               // User: ALICE
logger.info("Size: {bytes|bytes}", 1048576);               // Size: 1.0 MB
logger.info("Elapsed: {ms|duration}", 3661000);            // Elapsed: 1h 1m 1s
logger.info("Total: {amount:.2f|comma}", 1234567.89);     // Total: 1,234,567.89
```

### Syntax

- **Single transform:** `{name|transform}`
- **Chaining (left-to-right):** `{name|trim|upper|truncate:20}`
- **With format spec (spec applied first):** `{name:.2f|comma}`
- **With operators:** `{@name|upper}`, `{$name|quote}`

### Built-in Transforms

**String transforms:**

| Transform | Description | Example |
|-----------|-------------|---------|
| `upper` | ASCII uppercase | `{v\|upper}` — `"hello"` → `HELLO` |
| `lower` | ASCII lowercase | `{v\|lower}` — `"HELLO"` → `hello` |
| `trim` | Strip leading/trailing whitespace | `{v\|trim}` — `"  hi  "` → `hi` |
| `truncate:N` | Limit to N UTF-8 chars, append … | `{v\|truncate:5}` — `"Hello World"` → `Hello…` |
| `pad:N` | Right-pad with spaces to N chars | `{v\|pad:10}` — `"hi"` → `hi········` |
| `padl:N` | Left-pad with spaces to N chars | `{v\|padl:8}` — `"42"` → `······42` |
| `quote` | Wrap in double quotes | `{v\|quote}` — `hello` → `"hello"` |

**Number transforms:**

| Transform | Description | Example |
|-----------|-------------|---------|
| `comma` | Thousands separator | `{v\|comma}` — `1234567` → `1,234,567` |
| `hex` | Hex with 0x prefix | `{v\|hex}` — `255` → `0xff` |
| `oct` | Octal with 0 prefix | `{v\|oct}` — `8` → `010` |
| `bin` | Binary with 0b prefix | `{v\|bin}` — `10` → `0b1010` |
| `bytes` | Human-readable byte size | `{v\|bytes}` — `1048576` → `1.0 MB` |
| `duration` | Human-readable time from ms | `{v\|duration}` — `3661000` → `1h 1m 1s` |
| `pct` | Percentage (×100, append %) | `{v\|pct}` — `0.856` → `85.6%` |

**Structural transforms:**

| Transform | Description | Example |
|-----------|-------------|---------|
| `json` | JSON serialization | `{v\|json}` — `hello` → `"hello"` |
| `type` | Detected type name | `{v\|type}` — `42` → `int` |
| `expand` | Operator alias for `@` (destructure) | Affects JSON/XML properties |
| `str` | Operator alias for `$` (stringify) | Affects JSON/XML properties |

Non-numeric values pass through number transforms unchanged. Unknown transforms are ignored (fail-open).

### Chaining

Transforms are applied left-to-right. Format specifiers are applied **before** pipe transforms:

```cpp
logger.info("{val|trim|upper|truncate:10}", "  Hello, World!  ");
// Step 1 (trim):        "Hello, World!"
// Step 2 (upper):       "HELLO, WORLD!"
// Step 3 (truncate:10): "HELLO, WOR…"

logger.info("{revenue:.2f|comma}", 9876543.21);
// Step 1 (spec .2f):  "9876543.21"
// Step 2 (comma):     "9,876,543.21"
```

## Rolling File Sink

Production-grade log file rotation:

```cpp
// Size-based: rotate at 10MB, keep 5 files
logger.addSink<minta::RollingFileSink>(
    minta::RollingPolicy::size("logs/app.log", 10 * 1024 * 1024).maxFiles(5));

// Daily rotation, keep 30 days
logger.addSink<minta::RollingFileSink>(
    minta::RollingPolicy::daily("logs/app.log").maxFiles(30));

// Hybrid: daily + 50MB cap, total budget 500MB
logger.addSink<minta::RollingFileSink>(
    minta::RollingPolicy::daily("logs/app.log")
        .maxSize(50 * 1024 * 1024).maxTotalSize(500 * 1024 * 1024));
```

See the [wiki](https://github.com/LunarECL/LunarLog/wiki/Rolling-File-Sink) for full documentation.

## Compact Filters

Grep-inspired shorthand for common filter patterns:

```cpp
logger.filter("WARN+ ~timeout !~heartbeat");     // level + keyword + negation
logger.filter("ctx:env=prod");                    // context key=value
logger.sink("errors").filter("ERROR+");           // per-sink

// Equivalent to:
// logger.addFilterRule("level >= WARN");
// logger.addFilterRule("message contains 'timeout'");
// logger.addFilterRule("not message contains 'heartbeat'");
```

Space-separated = AND. See the [wiki](https://github.com/LunarECL/LunarLog/wiki/Compact-Filter) for full syntax.

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

## Filtering

LunarLog supports multi-layer filtering to control which log entries reach each sink. The full pipeline runs in order: global min level → global predicate → global DSL rules → per-sink min level → per-sink predicate → per-sink DSL rules.

### Per-Sink Log Levels

Each sink has its own minimum log level. The effective level for a sink is the stricter of the global logger level and the per-sink level:

```cpp
logger.addSink<minta::FileSink>("errors.log");
logger.addSink<minta::FileSink>("all.log");

logger.setSinkLevel(0, minta::LogLevel::ERROR);  // sink 0: errors only
// sink 1 uses the default (TRACE) — receives everything
```

### Predicate Filters

Set a custom filter function on the logger (global) or on individual sinks:

```cpp
// Global: only log WARN+ or entries marked important
logger.setFilter([](const minta::LogEntry& entry) {
    return entry.level >= minta::LogLevel::WARN ||
           entry.customContext.count("important") > 0;
});

// Per-sink: suppress sensitive data from sink 0
logger.setSinkFilter(0, [](const minta::LogEntry& entry) {
    return entry.message.find("sensitive") == std::string::npos;
});

logger.clearFilter();        // remove global predicate
logger.clearSinkFilter(0);   // remove sink 0 predicate
```

If the global predicate throws, the entry is let through (fail-open) rather than silently dropped for all sinks.

### DSL Filter Rules

Add declarative filter rules parsed from strings. Multiple rules are AND-combined — all must pass:

```cpp
logger.addFilterRule("level >= WARN");
logger.addFilterRule("not message contains 'heartbeat'");

// Per-sink DSL rules
logger.addSinkFilterRule(0, "context env == 'production'");
```

**Available conditions:**

| Condition | Example |
|---|---|
| `level >= LEVEL` | `level >= WARN` |
| `level == LEVEL` | `level == ERROR` |
| `level != LEVEL` | `level != TRACE` |
| `message contains 'text'` | `message contains 'error'` |
| `message startswith 'text'` | `message startswith 'ALERT'` |
| `template == 'exact'` | `template == 'User {name} logged in'` |
| `template contains 'partial'` | `template contains 'logged in'` |
| `context has 'key'` | `context has 'request_id'` |
| `context key == 'value'` | `context env == 'production'` |
| `not <rule>` | `not message contains 'noisy'` |

Level names: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`. String values must be single-quoted. Context keys with spaces are not supported in DSL rules.

### Clearing Filters

```cpp
logger.clearFilter();              // remove global predicate
logger.clearFilterRules();         // remove global DSL rules
logger.clearAllFilters();          // remove both global predicate and DSL rules

logger.clearSinkFilter(0);        // remove sink 0 predicate
logger.clearSinkFilterRules(0);   // remove sink 0 DSL rules
logger.clearAllSinkFilters(0);    // remove both predicate and rules from sink 0
```

### Filter Thread Safety

Filter predicates and DSL rules can be added, removed, or changed at any time — even while other threads are logging. Filter state is protected by internal mutexes. Per-sink filters copy state under lock and evaluate outside it, so slow predicates do not block other threads.

## Thread Safety

LunarLog is designed for concurrent use from multiple threads with the following contract:

- **Add all sinks before logging.** Calling `addSink()` after the first log message has been processed throws `std::logic_error`. Configure sinks during initialization only.
- **LunarLog must outlive all logging threads.** Destroying a `LunarLog` instance while other threads are still calling log methods is undefined behavior. Join or stop all producer threads before the logger goes out of scope.
- **Do not call `setFormatter`/`setTransport` after sink registration.** These methods are `protected` on `ISink` and should only be called from sink constructors. The `LunarLog` class accesses them internally via `friend` during `addSink<SinkType, FormatterType>(...)`.
- **`setContext` / `clearContext` / `clearAllContext`** are thread-safe and can be called concurrently with logging.
- **`setMinLevel` / `setCaptureSourceLocation`** are atomic and safe to call at any time.
- **`setFilter` / `clearFilter` / `addFilterRule` / `clearFilterRules` / `clearAllFilters`** and their per-sink equivalents are thread-safe and can be called concurrently with logging.
- **`ContextScope`** must not outlive the `LunarLog` instance it references. It holds a reference to the logger and calls `clearContext` in its destructor, so destroying the logger first is undefined behavior.

## Scoped Context (LogScope)

RAII scoped context — inject key-value pairs into log entries for the lifetime of the scope:

```cpp
{
    auto scope = logger.scope({{"requestId", "req-001"}, {"userId", "u-42"}});
    scope.add("role", "admin");
    logger.info("Processing request");
    // Output: Processing request [requestId=req-001, userId=u-42, role=admin]
}
// All context removed automatically
logger.info("No context here");
```

Nested scopes stack; inner scopes shadow outer scopes for duplicate keys:

```cpp
{
    auto outer = logger.scope({{"env", "outer"}});
    {
        auto inner = logger.scope({{"env", "inner"}});
        logger.info("Shadowed");  // env=inner
    }
    logger.info("Restored");      // env=outer
}
```

`LogScope` is move-only, thread-wide, and exception-safe. See the [wiki](https://github.com/LunarECL/LunarLog/wiki/Scoped-Context) for the full guide.

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

## Migration Notes

### Format Specifiers `:d`, `:f`, `:t`

The `:d`, `:f`, and `:t` format specifiers now produce locale-aware date/time output from Unix timestamps (see [Culture-Specific Formatting](#culture-specific-formatting) above). Previously these were no-ops that returned the value unchanged. If your templates use `{val:d}`, `{val:f}`, or `{val:t}` with non-timestamp values, the behavior has changed — non-numeric values are still returned as-is, but numeric values will now be interpreted as timestamps.

## License

MIT — see [LICENSE](LICENSE).
