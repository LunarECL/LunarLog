## What's New in v1.29.0

### Sync-First Core Architecture

LunarLog now dispatches log entries **synchronously on the caller's thread** by default. The internal async queue and consumer thread have been removed.

#### Before (v1.28.0 and earlier)

```
logger.info() → format → [internal queue] → consumer thread → sink
```

Every log call paid ~200ns queue overhead + flush required a round-trip sync (~1.8μs to null sink).

#### After (v1.29.0)

```
logger.info() → format → sink (direct call)
```

Logging is now a direct function call. No queue, no thread handoff, no flush overhead.

#### What This Means for You

- **Default behavior is synchronous** — log calls block until the sink finishes writing
- **Want async?** Wrap any sink with `AsyncSink` — this is the opt-in async decorator with bounded queue, overflow policies (Block/DropOldest/DropNewest), and flush sync
- **No API changes** — your existing code compiles and works as before
- **No double-async** — previously, using `AsyncSink` meant double-queuing (internal queue → AsyncSink queue). Now there's exactly one queue when you opt in.

#### Performance Impact

| Benchmark | v1.28.0 (async core) | v1.29.0 (sync core) | Improvement |
|-----------|---------------------|---------------------|-------------|
| Info SingleThread | 339 ns | 164 ns | **2x faster** |
| Flush Every 1 | 1,828 ns | 176 ns | **10x faster** |
| Flush Every 100 | 457 ns | 193 ns | **2.4x faster** |
| Trace Disabled | 11.7 ns | 11.7 ns | unchanged |

#### Async Example

```cpp
auto logger = minta::LunarLog::configure()
    .minLevel(minta::LogLevel::DEBUG)
    .writeTo<minta::AsyncSink>("async-console",
        minta::detail::make_unique<minta::ConsoleSink>(),
        minta::AsyncOptions{}.queueSize(8192).overflowPolicy(minta::OverflowPolicy::DropOldest))
    .build();

// Non-blocking — enqueued to AsyncSink's internal queue
logger.info("This won't block the caller");
```
