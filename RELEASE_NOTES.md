## What's New in v1.26.0

### CallbackSink — In-Process Callback Delivery (#84)

`CallbackSink` forwards each log entry to a user callback instead of writing to a file/console transport.

```cpp
// Variant 1: raw LogEntry callback
std::vector<minta::LogEntry> captured;
auto logger = minta::LunarLog::configure()
    .writeTo<minta::CallbackSink>(
        minta::CallbackSink::EntryCallback(
            [&](const minta::LogEntry& e) { captured.push_back(e); }))
    .build();

logger.info("Order {id} created", "id", "ORD-1001");
```

```cpp
// Variant 2: formatted string callback (CompactJsonFormatter by default)
std::vector<std::string> lines;
auto logger = minta::LunarLog::configure()
    .writeTo<minta::CallbackSink>(
        minta::CallbackSink::StringCallback(
            [&](const std::string& s) { lines.push_back(s); }))
    .build();

logger.warn("Queue depth {depth}", "depth", 42);
```

Highlights:
- `EntryCallback` for direct `LogEntry` access
- `StringCallback` with optional custom formatter (`CompactJsonFormatter` default)
- Synchronous callback execution, `flush()` no-op

### ConsoleStream — stdout/stderr Selection (#85)

`ConsoleSink` and `ColorConsoleSink` now accept `ConsoleStream` to route output to `stdout` or `stderr`.

```cpp
auto logger = minta::LunarLog::configure()
    .writeTo<minta::ConsoleSink>(minta::ConsoleStream::StdOut)
    .writeTo<minta::ColorConsoleSink>("err", minta::ConsoleStream::StdErr)
    .build();

logger.info("Service started on {port}", "port", 8080);   // stdout sink
logger.error("Failed to connect to {host}", "host", "db"); // stderr sink also receives
```

Highlights:
- `enum class ConsoleStream { StdOut, StdErr }`
- Stream choice is constructor-level, no transport customization needed
- Color console keeps TTY/NO_COLOR behavior per selected stream

### Global Static Logger — `minta::Log` (#86)

`minta::Log` provides process-wide static logging without passing logger references.

```cpp
#include <lunar_log/global.hpp>

int main() {
    minta::Log::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .writeTo<minta::ConsoleSink>()
        .build();

    minta::Log::info("App started: {name}", "name", "billing");
    LUNAR_GWARN("High latency: {ms}", "ms", 245);

    minta::Log::flush();
    minta::Log::shutdown();
}
```

Highlights:
- Static API: `configure`, `init`, `shutdown`, `instance`, `flush`, level methods, `log`
- Exception overloads mirrored from `LunarLog`
- Convenience global macros: `LUNAR_GTRACE` ~ `LUNAR_GFATAL`

---

## What's New in v1.25.0

### AsyncSink — Non-Blocking Sink Decorator (#74)

`AsyncSink<SinkType>` wraps an existing sink with a bounded queue and a dedicated consumer thread. Producers enqueue log entries quickly, and the inner sink writes on a background thread.

```cpp
minta::AsyncOptions opts;
opts.queueSize = 4096;
opts.overflowPolicy = minta::OverflowPolicy::DropOldest;
opts.flushIntervalMs = 1000;

auto logger = minta::LunarLog::configure()
    .writeTo<minta::AsyncSink<minta::FileSink>>(opts, std::string("app.log"))
    .build();

logger.info("Request {id} completed", "id", "req-123");
logger.flush();
```

Highlights:
- Overflow policies: `Block`, `DropOldest`, `DropNewest`
- Flush waits until enqueued entries are fully processed
- Works as a drop-in wrapper around existing sink types

### BatchedSink — Reusable Batch Delivery Base (#75)

`BatchedSink` is an abstract base sink for destinations that benefit from grouped writes (network, DB, remote collector). Implement `writeBatch()` and let the base class handle buffering, periodic flush, and retries.

```cpp
class MyBatchedSink : public minta::BatchedSink {
public:
    MyBatchedSink()
        : minta::BatchedSink(minta::BatchOptions()
              .setBatchSize(100)
              .setFlushIntervalMs(3000)
              .setMaxRetries(3)) {}

    ~MyBatchedSink() noexcept override { stopAndFlush(); }

protected:
    void writeBatch(const std::vector<const minta::LogEntry*>& batch) override {
        // send batch
    }
};
```

Highlights:
- `BatchOptions` for `batchSize`, interval, queue bound, retry policy
- Automatic periodic flush thread (`flushIntervalMs_ > 0`)
- Retry hook (`onBatchError`) and post-flush hook (`onFlush`)

### SyslogSink — POSIX Syslog Integration (#76)

`SyslogSink` forwards logs to the host syslog daemon using `<syslog.h>`. Useful for system-integrated service logging on Linux/macOS/BSD.

```cpp
minta::SyslogOptions opts;
opts.setFacility(LOG_LOCAL0)
    .setLogopt(LOG_PID | LOG_NDELAY)
    .setIncludeLevel(true);

minta::LunarLog logger(minta::LogLevel::INFO, false);
logger.addSink<minta::SyslogSink>("my-daemon", opts);
logger.warn("Queue depth is {depth}", "depth", 42);
```

Highlights:
- Level mapping from `LogLevel` to syslog priorities
- Configurable facility and openlog flags
- Optional `[LEVEL]` prefix for message body

### HttpSink — Batched HTTP/HTTPS Delivery (#77)

`HttpSink` extends `BatchedSink` and sends batches as HTTP POST payloads. Entries are formatted as JSONL via `CompactJsonFormatter`.

```cpp
minta::HttpSinkOptions opts("https://logs.example.com/ingest");
opts.setHeader("Authorization", "Bearer token")
    .setContentType("application/x-ndjson")
    .setBatchSize(50)
    .setFlushIntervalMs(5000)
    .setMaxRetries(3)
    .setTimeoutMs(10000);

auto logger = minta::LunarLog::configure()
    .writeTo<minta::HttpSink>(opts)
    .build();

logger.error("Failed to connect to {host}", "host", "db-primary");
logger.flush();
```

Highlights:
- Configurable URL, headers, timeout, SSL verify, and batch/retry parameters
- POSIX: raw HTTP socket path + curl fallback for HTTPS
- Windows: WinHTTP transport

## Upgrade Notes

- No breaking API changes in existing logging flows.
- New sink headers:
  - `<lunar_log/sink/async_sink.hpp>`
  - `<lunar_log/sink/batched_sink.hpp>`
  - `<lunar_log/sink/syslog_sink.hpp>`
  - `<lunar_log/sink/http_sink.hpp>`

---

## What's New in v1.24.0

### ColorConsoleSink — ANSI-Colored Console Output (#71)

A drop-in alternative to `ConsoleSink` that colorizes the `[LEVEL]` bracket with ANSI escape codes. Each log level gets a distinct color; the message body is left uncolored.

```cpp
auto logger = minta::LunarLog::configure()
    .minLevel(minta::LogLevel::TRACE)
    .writeTo<minta::ColorConsoleSink>("color-console")
    .build();

logger.trace("Trace message");   // dim
logger.info("Info message");     // green
logger.warn("Warning!");         // yellow
logger.error("Error occurred");  // red
```

#### Design

- **Per-level colors** — TRACE (dim), DEBUG (cyan), INFO (green), WARN (yellow), ERROR (red), FATAL (bold red)
- **TTY auto-detection** — color disabled automatically when stdout is piped or redirected
- **`NO_COLOR` standard** — respects `NO_COLOR` env var (https://no-color.org/); any value disables color
- **`LUNAR_LOG_NO_COLOR`** — project-specific override; non-empty value disables color
- **Windows VTP** — `ENABLE_VIRTUAL_TERMINAL_PROCESSING` enabled automatically on Windows 10+
- **Runtime toggle** — `sink.setColor(true/false)` overrides auto-detection at any time
- **Fluent builder compatible** — works with `.writeTo<minta::ColorConsoleSink>("name")`

### GitHub Project Hygiene (#72)

- `CHANGELOG.md` — full project history in Keep a Changelog format with compare links
- `.github/PULL_REQUEST_TEMPLATE.md` — PR checklist (tests, single_include, examples, wiki, CHANGELOG, breaking changes, CI)
- `.github/ISSUE_TEMPLATE/bug_report.md` — version, platform, compiler, minimal repro template
- `.github/ISSUE_TEMPLATE/feature_request.md` — problem, solution, API sketch, alternatives

### Codecov Coverage Badge (#73)

- Codecov upload step added to CI coverage job
- Coverage badge added to README

## Highlights

- **844 tests**, 0 failures
- `ColorConsoleSink` is fully header-only and C++11 compatible
- MSVC fix: `#undef ERROR` after `<windows.h>` prevents macro collision with `LogLevel::ERROR`
