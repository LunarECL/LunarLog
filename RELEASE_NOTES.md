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
