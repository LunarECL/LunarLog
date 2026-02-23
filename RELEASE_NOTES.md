## What's New in v1.26.1

### Key-Value Argument Support for Message Templates

LunarLog now supports Serilog-style key-value argument pairs alongside the existing positional arguments. Both styles work — existing code is fully backward compatible.

```cpp
// Key-value pairs — self-documenting, order-independent
logger.info("User {name} from {ip}", "name", "alice", "ip", "10.0.0.1");

// Positional — still works exactly as before
logger.info("User {name} from {ip}", "alice", "10.0.0.1");

// Key-value with out-of-order keys
logger.info("{b} then {a}", "a", "first", "b", "second");
// → "second then first"

// Key-value with format specifiers and pipe transforms
logger.info("Price: {amount:.2f}", "amount", 3.14159);     // → "Price: 3.14"
logger.info("{name|upper}", "name", "alice");               // → "ALICE"
```

**Detection heuristic:** If `args.size() == 2 × placeholders` and at least one even-indexed arg matches a placeholder name, key-value mode activates. Otherwise, positional mode is used.

Works everywhere: `logger.info()`, `LUNAR_INFO()` macros, `minta::Log::info()` global logger, JSON/XML structured output.

### Documentation Audit

Comprehensive wiki audit and fixes across 9 files:
- **API-Reference**: corrected move semantics, protected method visibility, removed nonexistent `getLevelFromString`
- **Batched-Sink**: fixed `retryDelayMs_` default (100, not 1000), removed incorrect "pure virtual" claim
- **Callback-Sink**: fixed include path, replaced non-copyable `LogEntry` examples
- **Color-Console-Sink**: replaced nonexistent `getSink()` with `sink()`
- **Cookbook**: fixed `LogEntry` copy pattern in TestSink
- **HTTP-Sink**: corrected CompactJson level abbreviations (`WRN` not `Warning`), added `retryDelayMs` to options table
- **Rate-Limiting**: documented existing `setRateLimit()` API (was incorrectly claimed to not exist)
- **Syslog-Sink**: corrected Windows behavior, ident storage, and locking documentation
- **Migration-Guide**: fixed `LogEntry` constructor signature

Wiki Home reorganized into 7 categorized sections. Comparison page updated with spdlog v2.x column and glog deprecation notice.
