# Changelog

All notable changes to LunarLog are documented in this file.
Format based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

## [v1.26.0] — 2026-02-22

### Added
- `CallbackSink` (#84) — callback-based sink with `EntryCallback` / `StringCallback` variants for custom in-process handling
- `ConsoleStream` selection for `ConsoleSink` / `ColorConsoleSink` (#85) — choose `StdOut` or `StdErr`
- Global static logger facade `minta::Log` + `LUNAR_G*` macros (#86) — process-wide logger access without passing instances

## [v1.25.0] — 2026-02-21

### Added
- `AsyncSink` (#74) — bounded-queue async sink decorator with overflow policies (`Block`, `DropOldest`, `DropNewest`) and flush synchronization
- `BatchedSink` (#75) — reusable batching base sink with timer flush, retry hooks, and configurable `BatchOptions`
- `SyslogSink` (#76) — POSIX syslog sink with facility/logopt configuration and level mapping
- `HttpSink` (#77) — batched HTTP/HTTPS sink with `HttpSinkOptions`, custom headers, retries, and Compact JSON payload delivery

## [v1.24.0] — 2026-02-19

### Added
- `ColorConsoleSink` — ANSI-colored console output with per-level colors, TTY auto-detection, `NO_COLOR` / `LUNAR_LOG_NO_COLOR` env var support, and Windows VTP
- Codecov coverage badge and CI upload step
- GitHub issue/PR templates (bug report, feature request, pull request checklist)

## [v1.23.0] — 2026-02-19

### Changed
- Refactored `LogEntry` exception storage to `unique_ptr<ExceptionInfo>` — reduces per-entry memory on the non-exception path
- **Breaking**: custom formatters must use `entry.exception->type/message/chain` instead of `entry.exceptionType/exceptionMessage/exceptionChain`

## [v1.22.0] — 2026-02-18

### Changed
- Copy-on-write optimization for filter rules, template cache, and tag sets — eliminates per-entry heap allocations on read paths

## [v1.21.1] — 2026-02-18

### Added
- Flush-inclusive throughput benchmarks (FlushEvery1/100/1000)
- Auto-publish benchmark results to Wiki on master push

### Fixed
- Conan `auto_header_only` implementation
- `generate_single_header.py` now includes `macros.hpp`
- Bench workflow timeout (`timeout-minutes: 15`)

## [v1.21.0] — 2026-02-18

### Added
- Source location macros: `LUNAR_TRACE`, `LUNAR_DEBUG`, `LUNAR_INFO`, `LUNAR_WARN`, `LUNAR_ERROR`, `LUNAR_FATAL`
- Generic `LUNAR_LOG(logger, level, ...)` macro
- Exception variants: `LUNAR_ERROR_EX`, `LUNAR_FATAL_EX`, and all `_EX` macros
- `#define LUNAR_LOG_NO_MACROS` opt-out guard
- 23 new tests (823 total)

## [v1.20.0] — 2026-02-18

### Added
- Benchmark suite using Google Benchmark v1.9.1
- Benchmarks: throughput, formatting, filtering, sink I/O, enrichers, end-to-end latency
- CI benchmark workflow on every push/PR

## [v1.19.0] — 2026-02-18

### Added
- Fluent builder API: `LunarLog::configure().minLevel().writeTo<>().enrich().build()`
- `LoggerConfiguration` class with `writeTo`, `enrich`, `filter`, `filterRule`, `rateLimit`, `locale`
- Per-sink configuration via `SinkProxy` lambda in builder
- 800 tests passing

## [v1.18.0] — 2026-02-18

### Added
- Enricher system: `threadId()`, `processId()`, `machineName()`, `environment()`, `property()`, `fromEnv()`, `caller()`
- Custom enrichers via lambda
- Enricher precedence: enricher < setContext < scoped context
- 22 new tests (777 total)

## [v1.17.0] — 2026-02-18

### Added
- Exception attachment: `logger.error(ex, "msg {key}", "key", val)`
- Nested exception chain unwinding (depth limit: 20)
- Type name demangling on GCC/Clang
- All 6 log levels support exception attachment
- Output in all formatters: human-readable, JSON, compact JSON, XML, output template
- 55 new tests (755 total)

## [v1.16.0] — 2026-02-18

### Added
- Compact JSON formatter (CLEF) for log collection pipelines
- Short `@`-prefixed keys: `@t`, `@l`, `@mt`, `@m`, `@i`
- Level abbreviation: TRC/DBG/INF/WRN/ERR/FTL
- Flattened properties at top level
- `@` collision escaping
- Shared JSON utilities in `json_detail.hpp`
- 52 new tests (700 total)

## [v1.15.0] — 2026-02-17

### Added
- Alignment and padding in placeholders: `{name,20}` (right), `{name,-20}` (left)
- UTF-8 aware alignment (counts code points, not bytes)
- Width clamped to 1024 to prevent excessive allocation
- 74 new tests (648 total)

## [v1.14.0] — 2026-02-17

### Added
- Indexed parameters: `{0}`, `{1}` positional placeholders with reuse and mixed mode
- Works with format specifiers, pipe transforms, operators
- Structured output safety: deduplicated JSON properties, mapped XML keys
- 12 new tests (574 total)

## [v1.13.0] — 2026-02-17

### Added
- Output template system: `{timestamp}`, `{level}`, `{message}`, `{properties}`, `{threadId}`, `{source}`, `{template}`, `{newline}`
- Per-sink template customization via `SinkProxy::outputTemplate()`
- Alignment support in output template tokens
- 58 new tests (562 total)

## [v1.12.0] — 2026-02-17

### Added
- Rolling file sink with size-based, time-based (daily/hourly), and hybrid rotation
- `RollingPolicy` fluent builder
- `maxFiles` and `maxTotalSize` cleanup policies
- Directory scan discovery on process restart
- Lazy file creation, auto directory creation
- 24 new tests (504 total)

## [v1.11.0] — 2026-02-17

### Added
- Compact filter syntax: `"WARN+ ~timeout !~heartbeat ctx:env=prod"`
- Level, message, context, and template pattern matching
- `WARNING` accepted as alias for `WARN`
- 73 new tests (480 total)

## [v1.10.0] — 2026-02-17

### Added
- Scoped context via `LogScope` — RAII key-value pairs with nested shadowing
- `add()` fluent API, move-only semantics, exception-safe cleanup
- 25 new tests (407 total)

## [v1.9.0] — 2026-02-17

### Added
- Pipe transform system: `{name|upper|truncate:10}`
- 18 built-in transforms: `upper`, `lower`, `trim`, `truncate`, `pad`, `padl`, `quote`, `comma`, `hex`, `oct`, `bin`, `bytes`, `duration`, `pct`, `expand`, `str`, `json`, `type`
- 79 new tests (382 total)

## [v1.8.0] — 2026-02-17

### Added
- Named sinks with `named()` factory and string lookup
- `SinkProxy` fluent API for per-sink configuration
- Tag-based routing: `[audit][security]` prefixes in message templates
- Only/except tag filtering
- Tags serialized in JSON arrays and XML elements
- 50 new tests (303 total)

## [v1.7.0] — 2026-02-16

### Added
- 12 standalone examples covering every feature
- Contributing guide (`CONTRIBUTING.md`)
- GitHub Wiki with 15 pages: API Reference, Getting Started, Cookbook, etc.

## [v1.6.0] — 2026-02-16

### Added
- Culture-specific formatting: `:n`/`:N` (locale-aware numbers), `:d`/`:D`/`:t`/`:T`/`:f`/`:F` (date/time)
- Per-sink locale overrides
- `thread_local` locale cache
- 253 tests total

### Changed
- `:d`, `:f`, `:t` specs now produce date/time from Unix timestamps (previously no-ops)

## [v1.5.0] — 2026-02-16

### Added
- Per-sink log levels via `setMinLevel()`
- Lambda predicate filters
- DSL filter rules: `level >= WARN`, `message contains 'error'`, `context has 'requestId'`
- Negation support, `clearAllFilters()`
- 197 tests total

## [v1.4.0] — 2026-02-16

### Added
- Structured template output: JSON/XML emit `messageTemplate` and `templateHash`
- Template caching (configurable, default 128)
- Enhanced validation: warns on duplicate names, empty placeholders, whitespace-only names
- 154 tests total

## [v1.3.0] — 2026-02-16

### Added
- `@` (destructure) and `$` (stringify) operators per MessageTemplates spec
- Operators combine with format specs: `{@amount:.2f}`
- 132 tests total

## [v1.2.0] — 2026-02-15

### Added
- Format specifiers: `.2f`, `X`/`x`, `C`, `P`, `e`/`E`, `04`
- 92 tests (up from 35)

### Fixed
- Lost-wakeup race in `flush()` that could deadlock under load
- Custom sinks that throw no longer crash the process
- File transport retries on I/O errors

## [v1.1.0] — 2026-02-14

### Added
- Single-header distribution: `single_include/lunar_log.hpp`
- Generator script: `python3 tools/generate_single_header.py`

### Changed
- CI cleanup — fixed duplicate workflow runs on PR branches

## [v1.0.0] — 2026-02-14

### Added
- Initial stable release
- Message templates with named placeholders (messagetemplates.org spec)
- Pluggable formatters: human-readable, JSON, XML
- Pluggable sinks: console, file
- Thread-safe logging with atomic log manager
- Rate limiting
- Global and scoped context capture
- C++11/14/17 support (GCC, Clang, AppleClang, MSVC)

[Unreleased]: https://github.com/LunarECL/LunarLog/compare/v1.25.0...HEAD
[v1.24.0]: https://github.com/LunarECL/LunarLog/compare/v1.23.0...v1.24.0
[v1.23.0]: https://github.com/LunarECL/LunarLog/compare/v1.22.0...v1.23.0
[v1.22.0]: https://github.com/LunarECL/LunarLog/compare/v1.21.1...v1.22.0
[v1.21.1]: https://github.com/LunarECL/LunarLog/compare/v1.21.0...v1.21.1
[v1.21.0]: https://github.com/LunarECL/LunarLog/compare/v1.20.0...v1.21.0
[v1.20.0]: https://github.com/LunarECL/LunarLog/compare/v1.19.0...v1.20.0
[v1.19.0]: https://github.com/LunarECL/LunarLog/compare/v1.18.0...v1.19.0
[v1.18.0]: https://github.com/LunarECL/LunarLog/compare/v1.17.0...v1.18.0
[v1.17.0]: https://github.com/LunarECL/LunarLog/compare/v1.16.0...v1.17.0
[v1.16.0]: https://github.com/LunarECL/LunarLog/compare/v1.15.0...v1.16.0
[v1.15.0]: https://github.com/LunarECL/LunarLog/compare/v1.14.0...v1.15.0
[v1.14.0]: https://github.com/LunarECL/LunarLog/compare/v1.13.0...v1.14.0
[v1.13.0]: https://github.com/LunarECL/LunarLog/compare/v1.12.0...v1.13.0
[v1.12.0]: https://github.com/LunarECL/LunarLog/compare/v1.11.0...v1.12.0
[v1.11.0]: https://github.com/LunarECL/LunarLog/compare/v1.10.0...v1.11.0
[v1.10.0]: https://github.com/LunarECL/LunarLog/compare/v1.9.0...v1.10.0
[v1.9.0]: https://github.com/LunarECL/LunarLog/compare/v1.8.0...v1.9.0
[v1.8.0]: https://github.com/LunarECL/LunarLog/compare/v1.7.0...v1.8.0
[v1.7.0]: https://github.com/LunarECL/LunarLog/compare/v1.6.0...v1.7.0
[v1.6.0]: https://github.com/LunarECL/LunarLog/compare/v1.5.0...v1.6.0
[v1.5.0]: https://github.com/LunarECL/LunarLog/compare/v1.4.0...v1.5.0
[v1.4.0]: https://github.com/LunarECL/LunarLog/compare/v1.3.0...v1.4.0
[v1.3.0]: https://github.com/LunarECL/LunarLog/compare/v1.2.0...v1.3.0
[v1.2.0]: https://github.com/LunarECL/LunarLog/compare/v1.1.0...v1.2.0
[v1.1.0]: https://github.com/LunarECL/LunarLog/compare/v1.0.0...v1.1.0
[v1.0.0]: https://github.com/LunarECL/LunarLog/releases/tag/v1.0.0
