## What's New in v1.27.0

### Sub-Logger / Nested Pipeline (#82)

Create sub-loggers with their own filters, enrichers, and sinks — enabling complex routing scenarios beyond tag-based filtering.

```cpp
auto logger = LunarLog::configure()
    .writeTo<ConsoleSink>("console")
    .writeTo<FileSink>("app-log", "app.log")
    .subLogger("errors", [](SubLoggerConfiguration& sub) {
        sub.filter("ERROR+")
           .enrich(Enrichers::property("pipeline", "error-alerts"))
           .writeTo<CallbackSink>("alert", [](const std::string& msg) {
               sendAlert(msg);
           });
    })
    .subLogger("audit", [](SubLoggerConfiguration& sub) {
        sub.filter("INFO+ ~audit")
           .writeTo<FileSink, JsonFormatter>("audit-trail", "audit.jsonl");
    })
    .build();
```

**Key design points:**
- **SubLoggerSink** is an ISink that owns a mini pipeline — composable with `AsyncSink` for non-blocking sub-pipelines
- **Independent filters**: sub-logger filters evaluate after the parent's global filter
- **Independent enrichers**: sub-loggers can attach properties without affecting the parent
- **Error isolation**: a throwing sub-sink doesn't crash the parent or sibling sub-loggers
- **No circular references**: sub-loggers cannot reference the parent

**Filter layering:** `minLevel()` gates at the ISink level (before `write()`), while `filter()`/`filterRule()` apply inside the sub-pipeline. Both can be combined.

Also works with the global logger:

```cpp
minta::Log::configure()
    .writeTo<ConsoleSink>("console")
    .subLogger("errors", [](SubLoggerConfiguration& sub) {
        sub.filter("ERROR+")
           .writeTo<CallbackSink>("alert", callback);
    })
    .build();

LUNAR_GERROR("Something went wrong: {}", reason);
// → console + error sub-logger both receive this
```

### Test Coverage

34 new tests covering filtering, enrichers, nesting, async wrapping, exception propagation, tag routing, concurrency, error isolation, and auto-naming.
