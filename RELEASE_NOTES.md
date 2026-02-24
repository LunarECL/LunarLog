## What's New in v1.28.0

### Dynamic Log Level / Runtime Configuration Reload (#83)

Change log levels, per-sink levels, and filter rules at runtime without restarting your application.

#### LevelSwitch — Shared Observable Level

```cpp
auto levelSwitch = std::make_shared<LevelSwitch>(LogLevel::INFO);
auto logger = LunarLog::configure()
    .minLevel(levelSwitch)
    .writeTo<ConsoleSink>("console")
    .build();

logger.debug("This won't show");      // filtered by INFO

levelSwitch->set(LogLevel::DEBUG);     // takes effect immediately

logger.debug("Now this shows!");       // DEBUG is now visible
```

Share a `LevelSwitch` between multiple loggers for coordinated level changes.

#### Config File Watcher

```cpp
auto logger = LunarLog::configure()
    .minLevel(LogLevel::INFO)
    .writeTo<ConsoleSink>("console")
    .writeTo<FileSink>("file", "app.log")
    .watchConfig("lunarlog.json", std::chrono::seconds(5))
    .build();
```

The watcher polls the config file every N seconds and applies changes atomically:

```json
{
    "minLevel": "DEBUG",
    "sinks": {
        "console": { "level": "WARN" },
        "file": { "level": "DEBUG" }
    },
    "filters": ["INFO+ !~heartbeat"]
}
```

**Graceful degradation:** If the config file is malformed or missing, current settings are kept and a warning is logged.

#### Features

- **Atomic level changes** — `LevelSwitch::set()` is thread-safe, takes effect immediately
- **Per-sink level override** — adjust individual sink verbosity via config
- **Filter hot-reload** — replace filter rules atomically using COW
- **Polling-based** — no platform-specific dependencies (inotify/FSEvents)
- **Case-insensitive** — config accepts `"debug"`, `"DEBUG"`, `"Debug"`
- **Minimal JSON parser** — header-only, no external dependencies, RFC 8259 compliant
- **C++11 compatible**

### Test Coverage

40 new tests: JSON parser (12), LevelSwitch (6), config watcher (6), builder integration (2), level parsing (2), config format (2), stress/concurrency (10).
